#include <stdio.h>

static unsigned int sentinel = 0xdeadbeef;


int
main (int argc, char** argv)
{
    int tmp = sentinel;

    printf("%x\n", tmp);

    sentinel = (0x12345678);

    printf("%x\n", sentinel);

    return 0;
}
