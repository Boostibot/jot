# jot
*A minimal base layer*

Everything I need from the standard library from the ground up while being as simple as possible. I belive that if the implementation is short and well written, the trouble of modifying it to suit particular needs is lesser than the combined developement cost & inherent untestability of a fully general solution.

## Arrays as the universal interface

It can be tempting to design iterator abstractions that will fill every need one will ever have. However it is often (always?) far more useful to use the simplest yet sufficient concept. In data stuctures that concept are dense arrays. Surprisingly most other structures can be implemented at least partially within arrays. 

For example hash tables are just separate arrays for keys, values and the jump table. This allows us to trivially expose and modify just the values or just the keys. This also means iterating the datstructure is the fastest it can be - just a regular for loop. Similarly handle tables, queues, linked lists can all be implemented within an array.

```cpp
String_Hash<int> hash_table;
set(&hash_table, own("Hello"), 4096);
set(&hash_table, own("World"), 256);
//and so on....

Slice<int> ints = values(&hash_table);
for(int i = 0; i < ints.size; i++)
  printf("%d ", ints[i]);
```

This lets us be incredibly specific and avoid writing generic functions, which in turn makes them easy to debug, understand and modify. 

## Hierarchical resources management

In higher level languages people like to talk of pieces of code (classes) 'owning' or 'being responsible' for a resource, making sure that under no circumstance it is allowed to leak. It is a lot harder to write these managers than regular code and a lot harder still to make sure there really are no corner cases. If the owner makes a single mistake - an implementation error under unexpected exception - the resource is leaked. This style also leads to black boxing.

We embrace the fact that things break and resources get leaked. Small pieces of code are still responsible for their resources however if they leak the damage is only local. All resources are obtained from a manager somewhere up the call stack, which in turn gets its from its manager and so on. Only at the end of this chain is the system layer. You as the programer are only responsible for keeping track of the resources on your level. If you are not thats your problem since you wont have access to them anymore but they never leak globally (the level above you still has full acess to them). 

An example of this are all of the allocators and file handles. You can localy change the default allocator for the layer below you by simply doing the following:

```cpp
My_Custom_Allocator alloc;
{
  memory_globals::Allocator_Swap alloc_swap(&alloc);
  //From now on all allocations will pass through My_Custom_Allocator...
  call_some_function_using_allocators();
}
```

The fact that most actual OS or hardware resources take a lot of time to acquire, doing this hierarchical wrapping poses very little performance overhead. In fact often the extra layer only acts as a cache making the acquisition actually faster. An example are all of the linear allocators.

## Open open principle

Because of the resource management strategy described above, we dont have to be so protective of our structures. Every struct field and every function is publically accessible. This makes the code trivial to extend and to modify which is great for just about everyone. We can still define interfaces and adhere to them but we can simply write our own function when they are not sufficient. This also means our interfaces can be a lot simpler.

I still however separate between two kinds of structures in code: the open struct which has every field freely modifiable and the closed struct which should not generally be modified unless you know what you are doing. The closed structs have all of their fields prefixed with underscore. An example of an open struct is Slice simply defined as:

```cpp
template<typename T>
struct Slice
{
    T* data = nullptr;
    isize size = 0;
}
```

## Minimal context

There are surprisingly few metrics for code quality. I like to use line count/frature count, the total number of exposed concepts and performance. However here we will talk about the time metric. What is the minimum time that is required for someone who is not familiar with the codebase to **FULLY** understand any given piece of code? What is the time to debug it? To modify it? Here a stress is put on the word fully. We do not care if the programmer undestands that the function `draw_rect()` probably draws a rectangle somewhere. Instead we care about her being able to understand: what are the necessary compoments for the function? What is the state the function needs to function? What are all the possible effects the function can have? - AND - How does the function work? I believe the simplest way to be able to answer all of these is just to understand the code itself. 

How to understand the code quickly? What is stopping us? Everyone is familiar with the for loop construct but almost nobody will be familiar with our specific implementation of the map function on arrays. This applies to evrything. We try to minimize the necessary knowledge of the rest of the codebase to be able to understand any given piece of it. We are minimizing the context one needs to hold in their head to work with the code. But not through hiding! By simply doing more with less.

This can be seen on the fact that the absolute majority of files are either completely standalone or are dependent only on `memory.h` for allocator support. This is extremely light weight and incredibly difficult to plan out correctly (see all the unrelated functions in `memory.h` for my struggles).
