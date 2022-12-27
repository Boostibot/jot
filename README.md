# jot
As simple as possible stl replacement focused on performence

While this library started as STL compatible simplification it quickly became apparent that the STL is far too complex for what it achives. As such this is an attempt at offering comparable levels of generality while also being transparent and easily debugible. It should be more or less apparent what any piece of code does and how to modify it. It is desirable to keep the levels of indirection, complexity and line count as low as possible.

# Remarks

1) The library keeps all object fields public and uses free functions as much as possible. This makes all object trivial to extend. Contradictory to what is tough this rarely poses problems. The philosophy is 'if it is unknown what given field does it should not be edited'. This is fairly simple to adhere to and keeps us from defining needless getters and setters which in turn reduces the number of API indirections. Simplicity is desired.

2) To increase expressivity the library doesnt use mutable references instead relying on pointers (generally assumed to be not null or indicated with the `Nullable<>` wrapper). This makes mutation visible to the caller of the function which references and memebr functions do not. 

3) The library declares its own inerfaces. These can be found in `jot/standards.h` and include `Assignable<>`, `Failable<>`, `Swappable<>` and more. These structs are specialized for all required types. Once specialized the type is said to be assignable/failable/swappable... and is fit to be used by the functions requiring such interface. These structs contain a single static function `perform` that needs to be provided by the specialization. This is required because concepts have strange declaration order semantics which makes them nigthmare to work with.  
