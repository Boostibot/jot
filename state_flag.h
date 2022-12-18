#pragma once
#include "types.h"
#include "meta.h"
#include "type_id.h"

namespace jot
{
    using state = type_id;

    template <auto val, class T = decltype(val)>
    constexpr auto get_state() -> state
    {
          return type_id_of(Const<val>);
    };
}