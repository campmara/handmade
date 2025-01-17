#include <Windows.h>

void foo(void)
{
    const char* bar = "This is the first thing we have actually printed.\n";

    // 'A' stands for ANSII. This function takes an ANSII string.
    OutputDebugStringA("Hello!!!\n");
}

struct projectile
{
    char unsigned is_on_fire; // 1 if on fire, 0 if not
    int damage; // how much damage it does on impact
    int particles_per_second;
    short how_many_cooks;

    // 1 byte + 4 bytes + 4 bytes + 2 bytes = 11 bytes
};

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    /*integer = 5;
    integer = 5 + 2;
    integer = integer + 7;

    char unsigned test;
    char unsigned *test_ptr;
    test_ptr = &test;
    test = 255;
    test = test + 1;

    foo();*/

    //unsigned int Test;
    //Test = 2'1000'1000'1000;

    projectile test;
    int size = sizeof(projectile);
    int size_of_test = sizeof(test);

    test.is_on_fire = 1;
    test.damage = 3247519;
    test.particles_per_second = 232323;
    test.how_many_cooks = 50;


}