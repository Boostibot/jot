#pragma once
#include <stdint.h>

inline void* operator new(size_t, void* ptr) noexcept;

namespace jot
{
    template <typename T>
    T make_def_value() noexcept
    {
        return T();
    }

    ///Calls deleter on the stored value in destructor
    template <typename T, void (*deleter)(T), T (*def_value)() = make_def_value<T>>
    struct Resource
    {
        //@NOTE: Takes def_value function instead of value
        // because not all types are be constxepr constructible
        T val = def_value();
        
        Resource() noexcept {};
        Resource(T val) noexcept : val((T&&) val) {}
        Resource(Resource const&) noexcept = delete;
        Resource(Resource&& other) noexcept : val((T&&) other.val)
        { 
            other.val = def_value();
        }
        
        ~Resource() noexcept
        {
            deleter((T&&) val);
        }

        Resource& operator=(Resource const&) noexcept = delete;
        Resource& operator=(Resource&& other) noexcept 
        {
            T temp = (T&&) other.val;
            other.val = (T&&) val;
            val = (T&&) temp;
            return *this;
        }
        
        operator T() noexcept {
            return val;    
        }
    };
    
    #ifdef RESOURCE_EXAMPLE
    struct Shader
    {
        static void program_delete(GLuint program) 
        { 
            if(program != -1)
                glDeleteProgram(program);
        };
        
        static GLuint program_def() 
        { 
            return (GLuint) -1;
        };

        //This program is now a resource and will get cleaned up automatically
        Resource<GLuint, program_delete, program_def> program;
        String_Builder vertex_path = {};
        String_Builder fragment_path = {};
        String_Builder geometry_path = {};
    };
    #endif
}
