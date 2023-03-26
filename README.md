# jot
*As simple as possible stl replacement focused on performence*

This is an attempt to rewrite everything I would need from the standard library from the ground up while being as simple as possible. This sometimes comes at a cost of lesser generality but makes up for it in performance and tersness. I belive that if the implementation is short and well written, the trouble of modifying it to suit particular needs is lesser than the combined developement cost & inherent untestability of a fully general solutions.

# Remarks

- The library keeps all object fields public and uses free functions as much as possible. This makes all object trivial to extend. Contradictory to what is tought this rarely poses problems. Fields that are not ment to be inetarcted with directly are prefixed by an underscore and acessed through appropriate functions. All other fields can be set and read arbitrarily. This is fairly simple to adhere to and keeps us from defining needless getters and setters which in turn reduces complexity.

- To increase expressivity the library doesnt use mutable references instead relying on pointers (generally assumed to be not null or indicated with the `Nullable<>` wrapper). This force the caller to explicitly comfirm the mutation (by using the & operator) which references and member functions do not. 

- The library declares inerfaces through specializing of templated structs with static methods. While this seems strange at first, it is fairly elegant solution and works exactly the same as traits in other langauges. The main ones can be found in `jot/standards.h` and include `Assignable<>`, `Failable<>`, `Swappable<>` and more. These structs are specialized for all required types. Once specialized the type is said to be assignable/failable/swappable... and is fit to be used by the functions requiring such interface.

- The entire library is non throwing. All errors are properly signaled to the called via the return value. I belive the burden of keeping track of another control path at all times is simply too error prone (at least for me) and is very difficult to properly test. The propagation of error values however is fairly difficult problem to solve generally. We simply allow each function to signal errors using an `open_enum` - a enum like numeric value that is guaranteed to be unique. (This is also how Zig handles errors)   

- The library is built with instrumentation in mind. Many containers and allocators have additional counter fields that make it very easy to debug the application later on. In addition liberal use of asserts makes hidden bugs surface as quickly as possible.

- Many of the included files are completely free standing. This one can simply copy their contents without including any additional files.

### Free standing files

The following files can be used without the rest of the library. They internaly mostly just include cstd headers.

- `array.h`
- `bits.h`
- `defer.h`
- `defines.h` & `undefs.h`
- `hash.h`
- `intrusive_list.h`
- `meta.h`
- `no_destruct.h`
- `open_enum.h`
- `slice.h` with `slice_operator_text.h`
- `time.h`
- `traits.h`
- `type_id.h`
- `types.h`
- `unistd.h`
- `utils.h`

