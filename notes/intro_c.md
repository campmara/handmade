# Intro to C on Windows

In the output, you might see a `1>` prefixing each output line. This indicates which logical processor the *compiler* is using (not your program).

Your computer's CPU comprises multiple cores, which each can manage multiple individual states, as if it contained more CPU cores inside of it. Very powerful! The "logical processors" are those individual hyperthreads.

Compilers themselves are also multithreaded, so that's why it indicates which logical processor is currently being used in the form of that `1>` indicator.

## Linking

Linking is when the compiler links references between files! It then packages all of those references into an .exe.