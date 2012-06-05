#include <stdio.h>
int
main (int argc, char** argv)
{
    unsigned int sentinel = 0xdeadbeef;
    
    printf("%x\n", sentinel);

    return 0;
}
