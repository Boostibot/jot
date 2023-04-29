# jot
*As simple as possible stl replacement focused on performence*

This is an attempt to rewrite everything I would need from the standard library from the ground up while being as simple as possible. This sometimes comes at a cost of lesser generality but makes up for it in performance and tersness. I belive that if the implementation is short and well written, the trouble of modifying it to suit particular needs is lesser than the combined developement cost & inherent untestability of a fully general solutions.

# Remarks

- The library keeps all object fields public and uses free functions as much as possible. This makes all object trivial to extend. Contradictory to what is tought this rarely poses problems. Fields that are not ment to be inetarcted with directly are prefixed by an underscore and acessed through appropriate functions. All other fields can be set and read arbitrarily. This is fairly simple to adhere to and keeps us from defining needless getters and setters which in turn reduces complexity.

- The library is built with instrumentation in mind. Many containers and allocators have additional counter fields that make it very easy to debug the application later on. 
In particular every allocation operation takes the current line information. This enables us to very easily debug all structure at runtime by just changing the allocator pointer.

- Most files depend only on `memory.h` which defined a common interface for allocation and can be easily replaced with `malloc`/`free`. This means it is incredibly easy to take and modify into your projects.

### Free standing files

The following files can be used without the rest of the library. They internaly mostly just include cstd headers.

- `memory.h`
- `static_array.h`
- `defer.h`
- `hash.h`
- `intrusive_list.h`
- `meta.h`
- `no_destruct.h`
- `open_enum.h`
- `slice.h` 
- `time.h`
- `traits.h`
- `type_id.h`
- `types.h`
- `unistd.h`
- `utils.h`

