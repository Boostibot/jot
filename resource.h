#pragma once
#include <memory>

namespace jot
{
    
    template <typename T>
    T make_def_value() noexcept
    {
        return T();
    }

    //Struct similar to std::unique_ptr which calls deleter on the
    // stored value in destructor. Is move only
    template <typename T, void (*deleter)(T), T (*def_value)() = make_def_value<T>>
    struct Resource
    {
        T val = def_value();
        
        Resource() noexcept = default;

        Resource(T val) noexcept
            : val(move(&val)) {}

        Resource(Resource const&) noexcept = delete;
        Resource(Resource&& other) noexcept 
        {
            *this = (Resource&&) other;
        }
        
        Resource& operator=(Resource const&) noexcept = delete;
        Resource& operator=(Resource&& other) noexcept 
        {
            T temp = move(&other.val);
            other.val = move(&this->val);
            this->val = move(&temp);
            return *this;
        }

        ~Resource() noexcept
        {
            deleter(move(&val));
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

        static T&& move(T* ptr)
        {
            return (T&&) *ptr;

        }
    };
    
    //use like so:
    #if 0
    struct Shader
    {
        static void program_delete(GLuint program) 
        { 
            if(program != -1)
                glDeleteProgram(program);
        };

        Resource<GLuint, program_delete> program = -1;
        String_Builder vertex_path = {};
        String_Builder fragment_path = {};
        String_Builder geometry_path = {};
    };
    #endif
}