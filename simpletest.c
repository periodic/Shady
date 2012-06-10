#include <stdio.h>
#include <stdlib.h>


static unsigned int testvar = 0xdeadbeef;

void
print_test()
{
    printf("Test var: 0x%x\n", testvar);
}

int
main (int argc, char** argv)
{
    int i;
    int* tmp = malloc(sizeof(int));

    for (i = 0; i < 4; i++)
    {
        printf("Old %i: %x\n", i, tmp[i]);
        tmp[i] = 0x10101010;
        printf("New %i: %x\n", i, tmp[i]);
    }

    for (i = 0; i < 4; i++)
    {
        print_test();
    }

    return 0;
}
