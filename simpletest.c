#include <stdio.h>

static unsigned int sentinel = 0xdeadbeef;
    
int
main (int argc, char** argv)
{
    int tmp = sentinel;

    printf("%x\n", tmp);

    int * f = 0xdeadbeef;

    printf("%p\n", f);

    return 0;
}
