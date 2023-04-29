#include "stack_trace.h"

#ifndef UNICODE
    #define UNICODE
#endif // !UNICODE

#include <windows.h>
#include <Psapi.h>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "dbghelp.lib")

// Some versions of imagehlp.dll lack the proper packing directives themselves
// so we need to do it.
#pragma pack( push, before_imagehlp, 8 )
#include <imagehlp.h>
#pragma pack( pop, before_imagehlp )

namespace jot
{
    struct Process_Module 
    {
        wString_Builder image_name;
        wString_Builder module_name;

        void* base_address = nullptr;
        isize load_size = 0;
        HMODULE module_handle = 0;
        bool loaded = false;
    };

    static
    Array<Process_Module> get_process_modules(HANDLE process);

    struct Debug_Context
    {
        HANDLE process = 0;
        HANDLE thread = 0;
        String_Builder search_path;
        Array<Process_Module> modules;
        bool is_init = false;
        DWORD error = 0;
        isize max_traces = 256;

        Debug_Context(String search_path = "") 
        {
            process = GetCurrentProcess();
            thread = GetCurrentThread();

            const char* csearch_path = nullptr;
            if(search_path.size != 0)
            {
                this->search_path = own(search_path);
                csearch_path = data(this->search_path);
            }

            if (!SymInitialize(process, csearch_path, false)) 
            {
                is_init = false;
                error = GetLastError();
                return;
            }

            DWORD symOptions = SymGetOptions();
            symOptions |= SYMOPT_LOAD_LINES | SYMOPT_UNDNAME;
            SymSetOptions(symOptions);

            modules = get_process_modules(process);
            for(Process_Module& module_data : modules)
            {
                bool load_state = SymLoadModuleExW(process, 0, data(module_data.image_name), data(module_data.module_name), (DWORD64)module_data.base_address, (DWORD) module_data.load_size, 0, 0);
                module_data.loaded = load_state;
                if(load_state == false)
                {
                    is_init = false;
                    error = GetLastError();
                }
            }
        }

        ~Debug_Context()
        {
            SymCleanup(process);
        }
    };
    

    static
    Array<Process_Module> get_process_modules(HANDLE process)
    {
        Array<Process_Module> modules;
        DWORD module_handles_size_needed = 0;
        HMODULE module_handles[256] = {0};
        TCHAR temp[4096] = {0};

        EnumProcessModules(process, module_handles, sizeof(module_handles), &module_handles_size_needed);
        resize(&modules, module_handles_size_needed/sizeof(HMODULE));

        for(isize i = 0; i < size(modules); i++)
        {
            HMODULE module_handle = module_handles[i];
            Process_Module module_data;
            MODULEINFO module_info;
            module_data.module_handle = module_handle;

            GetModuleInformation(process, module_handle, &module_info, sizeof(module_info));
            module_data.base_address = module_info.lpBaseOfDll;
            module_data.load_size = module_info.SizeOfImage;

            GetModuleFileNameEx(process, module_handle, temp, sizeof(temp));
            module_data.image_name = own(slice(temp));

            GetModuleBaseName(process, module_handle, temp, sizeof(temp));
            module_data.module_name = own(slice(temp));
        
            modules[i] = (Process_Module&&) module_data;
        }
    
        return modules;
    }
    
    static
    Array<void*> fill_stack_frames(CONTEXT context, Debug_Context* debug_context, DWORD image_type = 0, isize max_frames = (isize) 1 << 62)
    {
        STACKFRAME64 frame = {0};
        #ifdef _M_IX86
            DWORD native_image = IMAGE_FILE_MACHINE_I386;
            frame.AddrPC.Offset    = context.Eip;
            frame.AddrPC.Mode      = AddrModeFlat;
            frame.AddrFrame.Offset = context.Ebp;
            frame.AddrFrame.Mode   = AddrModeFlat;
            frame.AddrStack.Offset = context.Esp;
            frame.AddrStack.Mode   = AddrModeFlat;
        #elif _M_X64
            DWORD native_image = IMAGE_FILE_MACHINE_AMD64;
            frame.AddrPC.Offset    = context.Rip;
            frame.AddrPC.Mode      = AddrModeFlat;
            frame.AddrFrame.Offset = context.Rsp;
            frame.AddrFrame.Mode   = AddrModeFlat;
            frame.AddrStack.Offset = context.Rsp;
            frame.AddrStack.Mode   = AddrModeFlat;
        #elif _M_IA64
            DWORD native_image = IMAGE_FILE_MACHINE_IA64;
            frame.AddrPC.Offset    = context.StIIP;
            frame.AddrPC.Mode      = AddrModeFlat;
            frame.AddrFrame.Offset = context.IntSp;
            frame.AddrFrame.Mode   = AddrModeFlat;
            frame.AddrBStore.Offset= context.RsBSP;
            frame.AddrBStore.Mode  = AddrModeFlat;
            frame.AddrStack.Offset = context.IntSp;
            frame.AddrStack.Mode   = AddrModeFlat;
        #else
            #error "Unsupported platform"
        #endif

        if(image_type == 0)
            image_type = native_image; 
    
        isize frame_count = 0;
        Array<void*> frames;
        for(frame_count; frame_count < max_frames; frame_count++)
        {
            bool ok = StackWalk64(image_type, debug_context->process, debug_context->thread, &frame, &context, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL);
            if (ok == false)
                break;

            void* addr = (void*) frame.AddrPC.Offset;
            push(&frames, addr);
        }
    
        return frames;
    }

    static
    Array<Stack_Trace_Entry> process_stack_trace(Debug_Context* debug_context, Slice<void*> addr_array) 
    {
        constexpr isize max_name_len = 1024;
        constexpr isize total_symbol_info_size = sizeof(SYMBOL_INFO) + max_name_len + 1;
        
        char symbol_info_data[total_symbol_info_size] = {0};
        char symbol_name_data[max_name_len] = {0};

        Array<Stack_Trace_Entry> entries;

        DWORD offset_from_symbol=0;
        IMAGEHLP_LINE64 line = {0};
        line.SizeOfStruct = sizeof line;

        bool is_below_main = false;
        for(isize i = 0; i < addr_array.size; i++)
        {
            Stack_Trace_Entry entry;
            DWORD64 address = (DWORD64) addr_array[i];
            entry.address = address;
            entry.is_architectural = is_below_main;
            if (address != 0) 
            {
                memset(symbol_info_data, '\0', total_symbol_info_size);
                SYMBOL_INFO* symbol_info = (SYMBOL_INFO*) symbol_info_data;
                symbol_info->SizeOfStruct = sizeof(SYMBOL_INFO);
                symbol_info->MaxNameLen = max_name_len;
                DWORD64 displacement = 0;
                SymFromAddr(debug_context->process, address, &displacement, symbol_info);
            
                if (symbol_info->Name[0] != '\0')
                {
                    isize actual_size = UnDecorateSymbolName(symbol_info->Name, symbol_name_data, max_name_len, UNDNAME_COMPLETE);
                    entry.source_mangled_function = own(String((char*) symbol_info->Name));

                    if(actual_size != 0)
                    {
                        String fn = String{symbol_name_data, actual_size};
                        entry.source_function = own(fn);
                        if(fn == "RaiseException" || fn == "CxxThrowException")
                            entry.is_architectural = true;

                        if(fn == "main")
                            is_below_main = true;
                    }
                }
           
                IMAGEHLP_MODULE module_info = {0};
                module_info.SizeOfStruct = sizeof(IMAGEHLP_MODULE);
                bool module_info_state = SymGetModuleInfo64(debug_context->process, address, &module_info);
                if(module_info_state)
                {
                    entry.source_module = own(slice(module_info.ImageName));
                    entry.source_module_name = own(slice(module_info.ModuleName));
                }
            
                if (SymGetLineFromAddr64(debug_context->process, address, &offset_from_symbol, &line)) 
                {
                    entry.source_line = line.LineNumber;
                    entry.source_file = own(String(line.FileName));
                }
            }

            push(&entries, move(&entry));
        }

        return entries;
    }
    
    static
    Array<Stack_Trace_Entry> get_stack_trace(Debug_Context* debug_context, const EXCEPTION_POINTERS* exp_ptrs)
    {
        void *base = debug_context->modules[0].base_address;
        IMAGE_NT_HEADERS *image_header = ImageNtHeader(base);
        DWORD image_type = image_header->FileHeader.Machine;
    
        Array<void*> addr_array = fill_stack_frames(*exp_ptrs->ContextRecord, debug_context, image_type);
        return process_stack_trace(debug_context, slice(&addr_array));
    }
    
    static
    Array<Stack_Trace_Entry> get_stack_trace(Debug_Context* debug_context, isize skip_levels = 1, isize max_levels = -1)
    {
        if(max_levels < 0)
            max_levels = debug_context->max_traces; //large number

        Array<void*> addr_array;
        resize(&addr_array, max_levels);
        DWORD hash;
        isize frames = CaptureStackBackTrace((DWORD) skip_levels, (DWORD) size(addr_array), data(&addr_array), &hash);
        resize(&addr_array, frames);
    
        return process_stack_trace(debug_context, slice(&addr_array));
    }
    
    static
    bool clamp_utf16_to_ascii(char* acii_string, const wchar_t* wide_str, size_t str_size, char if_not_supported = '?')
    {
        bool was_lossless = true;

        if(wide_str == nullptr || acii_string == nullptr)
            return was_lossless;
    
        for(size_t i = 0; i < str_size; i++)
        {
            if(wide_str[i] < 128)
                acii_string[i] = (char) wide_str[i];
            else
            {
                was_lossless = false;
                acii_string[i] = if_not_supported;
            }
        }

        return was_lossless;
    }

    struct Windows_Stack_Tracer : Stack_Tracer
    {
        Debug_Context debug_context;
        
        Windows_Stack_Tracer(String search_path = "")  
            : debug_context(search_path) 
        {}

        virtual Array<Stack_Trace_Entry> capture_stack_trace(isize skip_levels = 0, isize max_levels = -1) override 
        {
            Array<Stack_Trace_Entry> traces = get_stack_trace(&debug_context, skip_levels + 2, max_levels);
            //mark_traces_from_file_as_archutectural(&traces, "stack_trace_windows.h");
            return traces;
        }

        virtual bool protected_call(
            void (*protected_fn)(void* p_context), void* protected_context, 
            void (*fallback_fn)(void* f_context, Array<Stack_Trace_Entry> trace), void* fallback_context
        ) override
        {
            //in function using __try __except cannot be anything using destructors -> we use pointers instead
            struct local_fn
            {
                static DWORD process_and_call_exception(EXCEPTION_POINTERS *ep, Debug_Context* debug_context,
                    void (*fallback_fn)(void* f_context, Array<Stack_Trace_Entry> trace), void* fallback_context) 
                {
                    Array<Stack_Trace_Entry> traces = get_stack_trace(debug_context, ep);
                    mark_traces_from_file_as_archutectural(&traces, "stack_trace_windows.h");

                    fallback_fn(fallback_context, traces);

                    return EXCEPTION_EXECUTE_HANDLER;
                }
            };

            __try { 
                protected_fn(protected_context);
                return true;
            } __except (local_fn::process_and_call_exception(GetExceptionInformation(), &debug_context, fallback_fn, fallback_context)) {  
                return false;
            }
        
        }

        virtual ~Windows_Stack_Tracer() override {}
    };
}
