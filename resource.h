#pragma once

#include "defines.h"


#ifndef NDEBUG
#include <string>
#endif

namespace jot 
{
    template <typename Type, typename Label = void>
    struct MovedRes { Type res };

    template <typename Type, typename Label = void>
    struct Res 
    { 
        Type res;

        //force free
        #ifndef NDEBUG
        bool freed = false;
        #endif

        Res() = delete;
        Res(MovedRes<Type, Label> moved) : res(std::move(moved.res)) {}

        #ifndef NDEBUG
        ~Res() 
        {
            if(!freed)
                throw std::string("Leaing Res: ") + typeid(*pb).name();
        }
        #endif

        Res& operator = (const Type& _res) {return res = _res;}
        Res& operator = (Type&& _res) {return res = std::move(_res);}

        operator const Type&() const {return res;}
        operator Type&() {return res;}

        Type& operator*() {return res;}
        const Type& operator*() const {return res;}
        Type* operator->() {return &res;}
        const Type* operator->() const {return &res;}
    };


    template <typename T, typename L = void>
    func make() -> MovedRes<T, L>;

    template <typename T, typename L = void>
    proc drop(Res<T, L> res) -> void {}

    namespace unsafe 
    {
        template <typename T, typename L = void>
        func make(T val) {return Res<T, L>(MovedRes<T>(std::move(val));}

        template <typename T, typename L = void>
        func drop(Res<T, L>& val) {
            #ifndef NDEBUG
            res.freed = true;
            #endif
        }
    }

    template <typename T, typename L = void>
    func operator ~(Res<T, L>& res) -> MovedRes<T, L>
    {
        unsafe::drop(res);
        return MovedRes<T, L>{res.res};
    }

    template <typename T, typename L = void>
    proc operator ~(MovedRes<T, L> res) {} //consumes nodicard warning

}

#include "undefs.h"