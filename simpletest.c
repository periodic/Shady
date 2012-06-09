#include <stdio.h>
#include <stdlib.h>

int
main (int argc, char** argv)
{
    int i;
    int* tmp = malloc(sizeof(int));

    for (i = 0; i < 4; i++)
    {
        printf("Old %i: %x\n", i, tmp[i]);
        tmp[i] = i;
        printf("New %i: %x\n", i, tmp[i]);
    }

    return 0;
}
