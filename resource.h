#pragma once

namespace jot
{
    //Struct similar to std::unique_ptr which calls Deleter::use on the
    // stored value in destructor. Is move only
    template <typename T, typename Deleter>
    struct Resource
    {
        T val = T();

        Resource(T val = T()) noexcept
            : val(move(&val)) {}

        Resource(Resource const&) noexcept = delete;
        Resource(Resource&& other) noexcept 
        {
            *this = move(&other);
        }
        
        Resource& operator=(Resource const&) noexcept = delete;
        Resource& operator=(Resource&& other) noexcept 
        {
            T temp = move(&other.val);
            other.val = move(&this->val);
            this->val = move(&temp);
            return *this;
        }

        ~Resource()
        {
            Deleter::use(move(&val));
        }
        
        operator T() noexcept {
            return val;    
        }

        T& operator*() noexcept {
            return val;
        }
        
        T const& operator*() const noexcept {
            return val;
        }
    };
    
    #define PP_CONCAT2(a, b) a ## b
    #define PP_CONCAT(a, b) PP_CONCAT2(a, b)

    #define DELETER_FUNC(func) \
        struct PP_CONCAT(Deleter_Func_, __COUNTER__)\
        {   \
            static void use func \
        } \

    //use like so:
    #if 0
    struct Shader
    {
        using Program_Deleter = DELETER_FUNC((GLuint program) { 
            if(program != -1)
                glDeleteProgram(program);
        });

        Resource<GLuint, Program_Deleter> program = cast(GLuint) -1;
        String_Builder vertex_path = {};
        String_Builder fragment_path = {};
        String_Builder geometry_path = {};
    };
    #endif
}