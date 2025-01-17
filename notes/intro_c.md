# Intro to C on Windows

(Only really going to write down the new information I come across!)

In the output, you might see a `1>` prefixing each output line. This indicates which logical processor the *compiler* is using (not your program).

Your computer's CPU comprises multiple cores, which each can manage multiple individual states, as if it contained more CPU cores inside of it. Very powerful! The "logical processors" are those individual hyperthreads.

Compilers themselves are also multithreaded, so that's why it indicates which logical processor is currently being used in the form of that `1>` indicator.

## Linking

Linking is when the compiler links references between files! It then packages all of those references into an .exe.

Executable files allow Windows to know where in your program to actually start executing.

## WinMain

This is the main entrypoint function for a windows program!!

```c++
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    // Program goes here!
}
```

`CALLBACK` is a preprocessor macro that expands to something else.

`HINSTANCE` is a handle to our running program.

`hPrevInstance` is legacy and can be totally ignored.

`LPSTR` is what got sent to us when we were run! These are command line arguments.

`nShowCmd` corresponds to which option is selected (window start minimized, maximized, normal, etc.)

## Forward Declaration

You can declare a function signature before its definition:

```c++
// At the top of the file
void foo(void);

// other code...

// Somewhere else
void foo(void)
{
    // function body
}
```

This basically tells the linker that the function being forward declared is going to be defined later.

## Watch Windows

In the watch window (while debugging, Debug > Windows > Watch > Watch 1, 2, 3, 4), you can read memory in a const char* array by typing the variable name in the watch window field, followed by a comma and a number of bits you want to show from there. It will then display the memory under that variable for the given size.

## OutputDebugStringA, W, etc.

There are different function suffixes, implemented through macros, that change the output string type based on what we use. This is a vestige from when wide characters and unicode characters were developed.

## Types of numbers

* `char` is the smallest number, it is 8 bits
  * 00000000 <- represented like this in binary
  * ^ 256 different values (2possible values ^8 bits, or 2^8)
  * `char` goes from [-128, 127]
  * `unsigned char` goes from [0, 255]
* `short` is medium
  * 16 bits, so 65536 different possible values
* `int` is large
  * 32 bits, so ~4 billion possible values

Computers work with bits (binary 0s and 1s).

If you go beyond the extents of one of these primitive types, you'll notice the value 'wraps' around back to the minimum value. This is called overflowing the value.

## The `mov` instruction

Moves memory from one place to another!

Example assembly `mov` instruction
```
test = 255; // code expression
00007FF7F53E3B45  mov         byte ptr [test],0FFh
```

`0FFh` is a constant, meaning 255.

The `eax` register is 32 bits wide, which is why it can still express an overflowed unsigned char as '256' instead of 0. But as far as the program is concerned, that is now 0.

## Memory

Memory is the most important thing you will worry about as a programmer. You are telling the CPU to modify and read memory, at the barest level.

The more you *abstract* yourself from this (w/ OOP principles, etc.), the slower your program will run.

Machines work with what's called Virtual Memory. In the old days memory addresses in our programs were the actual physical memory addresses on the machine. Nowadays we use Virtual Memory, which allows us to cooperate with other programs. It prevents us from overwriting memory in other programs.

The way this works is by dividing the memory into pages. Pages are a certain range, like 4096 bytes. There are a certain amount of pages in physical memory, but each process has its own concept of these pages. The pages that a process accesses only exist when it actually accesses them. The OS is in charge of shuttling physical memory out to the hard drive or rearranging these pages. It uses a table maintained on the CPU that allows each process's view of these pages to be dynamically mapped to where they actually are in physical memory. When we talk about a location in memory in our code, we're actually talking about that location in our *virtual memory*. The actual memory location might be all over the place!!

It's important to know how this all works under the hood because if we're not careful we might force the OS to perform slow operations to account for our memory access. For example, the OS might have to swap that memory out if you use to much of it.

The Memory windows in Visual studio can show you a layout of the bytes in virtual memory (Debug > Windows > Memory > Memory 1, 2, 3, 4). In this window you can type in the address of any pointer in your program in order to see where the memory is and what its hexadecimal value is.

Note: You cannot touch memory you don't own yet. You need to request it from the OS in order to do that (not sure how yet, but I assume he's referring to VirtualAlloc).

How were we able to get memory for our program without explicitly asking for it? It happened automatically on the *Stack*. The compiler does the work for asking for the stack memory when you compile the program. When we declare a variable it starts grabbing memory from the bottom of the stack. When you reach the end of the curly brace, that memory is usually popped from the top of the stack. The major point is that memory is never added or removed from the middle of the stack. It's a stack! The reason why this works is because of the flow of writing programs in C: we're always doing a call and then a return. Call, memory allocated, return, memory freed.

Every time we run our program the stack is in a different place! The reason for this is for security, if memory was the same location-wise, hackers could exploit your program at specific memory locations. This is called ASLR (Address Space Location Randomization). We might turn this feature off occasionally for debugging purposes.

Memory is physically far away from the CPU on the motherboard, meaning that when the CPU needs to go and ask for something from memory, that's slow! We'd like to ideally have as many operations as possible occur on the CPU without going back and forth from the memory.

Two big things with respect to memory:
1. Latency
2. Throughput

Latency:
* How long it takes for operations to complete a loop around from Disk To Memory To CPU.

Throughput:
* How much can be processed at one time at one point in that journey.

## Cache

The CPU has pieces of memory that are right on the chips, called the Cache. When the process needs something from memory, it will first ask the cache if that piece of memory exists there. If it does, that's a *cache hit* and the CPU can perform its operations much quicker.

If, however, that piece of memory is not available in the cache, then the CPU has to request it from Memory, which is obviously much slower due to the physical distance the memory has to travel through the wires. This is called a *cache miss*.

Cache sizes are necessarily quite small (in Casey's example, an L3 cache had 8 megabytes of memory).

## Binary representation:

| 32768 | 16384 | 8192 | 4096  | 2048 | 1024 | 512 | 256  | 128 | 64  | 32  | 16   | 8   | 4   | 2   | 1   |
| ----- | ----- | ---- | ----- | ---- | ---- | --- | ---- | --- | --- | --- | ---- | --- | --- | --- | --- |
| 0     | 0     | 0    | 0   - | 0    | 0    | 0   | 0  - | 0   | 0   | 0   | 0  - | 0   | 0   | 0   | 0   |

## Endian-ness

Little Endian (x86, arm, x64):
* if the low order byte comes first in memory

Big Endian (Xbox360 and PS3, nowadays those are using newer processors that are little endian):
* if the high order byte comes first in memory

We still have to deal with Big endianness with working with other programs! Importing / exporting from stuff sometimes involves a different endianness.

## Why is garbage memory always 204?

204 is cc in hexadecimal!

A lot of errors come from uninitialized values. In debug builds, it's often a value that doesn't really appear deliberately, so in debug builds if you see this value and have a bug, that's likely what the bug is! Uninitialized values...

## Pointers are 4 bytes!

Just a random note. Remember this!

## Array syntax is shorthand!

```c
Projectile[30].Damage = 60;            // is shorthand for:
(ProjectilePointer + 30)->Damage = 60; // which is also shorthand for:
(ProjectilePointer + 30 * sizeof(projectile))->Damage = 30;
```
