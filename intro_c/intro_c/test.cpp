#include <Windows.h>

void foo(void)
{
    const char* bar = "This is the first thing we have actually printed.\n";

    // 'A' stands for ANSII. This function takes an ANSII string.
    OutputDebugStringA("Hello!!!\n");
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    int integer;

    integer = 5;
    integer = 5 + 2;
    integer = integer + 7;

    char unsigned test;
    char unsigned *test_ptr;
    test_ptr = &test;
    test = 255;
    test = test + 1;

    foo();
}