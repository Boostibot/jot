#pragma once

#ifndef NDEBUG
#include <string>
#endif

#include "defines.h"

namespace jot 
{
    template <class Type, class Label = void>
    struct Moved_Res { Type res; };

    template <class Type, class Label = void>
    struct Res 
    { 
        Type res;

        //force free
        #ifndef NDEBUG
        bool freed = false;
        #endif

        constexpr Res() = delete;
        constexpr Res(Moved_Res<Type, Label> moved) : res(std::move(moved.res)) {}

        #ifndef NDEBUG
        constexpr ~Res() noexcept(false)
        {
            if(!freed)
                throw std::string("Leaking Res: ") + typeid(res).name();
        }
        #endif

        constexpr Res& operator = (const Type& _res) {return res = _res;}
        constexpr Res& operator = (Type&& _res) {return res = std::move(_res);}

        constexpr operator const Type&() const {return res;}
        constexpr operator Type&() {return res;}

        constexpr Type& operator*() noexcept {return res;}
        constexpr const Type& operator*() const noexcept {return res;}
        constexpr Type* operator->() noexcept {return &res;}
        constexpr const Type* operator->() const noexcept {return &res;}
    };


    template <class T, class L = void>
    func make() -> Moved_Res<T, L>;

    template <class T, class L = void>
    proc drop(Res<T, L> res) noexcept -> void {}

    namespace unsafe 
    {
        template <class T, class L = void>
        func make(T val) noexcept
        {
            return Res<T, L>( Moved_Res<T, L>(std::move(val)) );
        }

        template <class T, class L = void>
        func drop(Res<T, L>& val) noexcept {
            #ifndef NDEBUG
            val.freed = true;
            #endif
        }
    }

    template <class T, class L = void>
    func operator ~(Res<T, L>& res) noexcept -> Moved_Res<T, L>
    {
        unsafe::drop(res);
        return Moved_Res<T, L>{res.res};
    }

    template <class T, class L = void>
    proc operator ~(Moved_Res<T, L> res) noexcept {} //consumes nodicard warning



    /*template <typename T>
    struct Opt_Ptr
    {
        private:
            T* val;

        public:
            constexpr get() const noexcept {return }
    };*/
}

#include "undefs.h"