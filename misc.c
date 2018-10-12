#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "misc.h"

typedef struct ARRAY_IS
{
    int nMemb;
    int blocks;
    int step;
    int membSize;
    void * mem;
} ARRAY_S;

ARRAY_PS arrayNew(int membSize, int step)
{
    CHECK_RTNM((membSize == 0 || step == 0), NULL, "parm error! membSize:%d step:%d\n", membSize, step);

    ARRAY_S * array = calloc(1, sizeof(ARRAY_S));
    CHECK_RTNM((array == NULL), NULL, "calloc error! membSize:%d %s\n", membSize, strerror(errno));

    array->nMemb = 0;
    array->blocks = step;
    array->step = step;
    array->membSize = membSize;
    array->mem = (void *)calloc(step, membSize);
    CHECK_GOTOM((array->mem == NULL), RELEASE, "calloc error! step:%d membSize:%d %s\n", step, membSize, strerror(errno));
    return (ARRAY_PS)array;

RELEASE:
    if (array)
    {
        free(array);
    }
    return NULL;
}

void * arrayAdd(ARRAY_PS array_ps, void * element)
{
    ARRAY_S * array = (ARRAY_S *)array_ps;
    CHECK_RTNM((array == NULL), NULL, "parm error! array:%#lx element:%#lx\n", (ADDR_T)array, (ADDR_T)element);

    if (array->nMemb >= array->blocks)
    {
        int newSize = array->blocks + array->step;
        void * newMem = (void *)realloc(array->mem, array->membSize * newSize);
        CHECK_RTNM((newMem == NULL), NULL, "realloc error! size:%d %s\n", array->membSize * newSize, strerror(errno));

        memset((char *)newMem + array->blocks * array->membSize,0,array->step * array->membSize);/* clear new memory */
        array->blocks = newSize;
        array->mem = newMem;
    }

    void * arrayElement = (void *)((char *)array->mem + array->nMemb * array->membSize);
    if(element)
    {
        memcpy(arrayElement, element, array->membSize);
    }
    array->nMemb++;

    return arrayElement;
}

int arrayExpandTo(ARRAY_PS array_ps, int size)
{
    ARRAY_S * array = (ARRAY_S *)array_ps;
    CHECK_RTNM((array == NULL), -1, "array invalid! \n");

    if (array->blocks < size)
    {
        void * newMem = (void *)realloc(array->mem, array->membSize * size);
        CHECK_RTNM((newMem == NULL), -1, "realloc error! size:%d %s\n", array->membSize * size, strerror(errno));

        memset((char *)newMem + array->blocks * array->membSize,0,(size - array->blocks)*array->membSize);/* clear new memory */
        array->blocks = size;
        array->mem = newMem;
    }

    return 0;
}

void * arrayMutable(ARRAY_PS array_ps, int * nMemb)
{
    ARRAY_S * array = (ARRAY_S *)array_ps;
    CHECK_RTNM((array == NULL), NULL, "array invalid! \n");

    if (nMemb)
    {
        *nMemb = array->nMemb;
    }

    return array->mem;
}

int arrayDel(ARRAY_PS array_ps)
{
    ARRAY_S * array = (ARRAY_S *)array_ps;
    CHECK_RTNM((array == NULL), -1, "array invalid! \n");

    if (array->mem)
    {
        free(array->mem);
    }

    free(array);

    return 0;
}

char * sstrcpy(char * dest, const char * src, int dsize)
{
    if (dest && src && (dsize > 0))
    {
        int copyLen = dsize - 1;
        strncpy(dest, src, copyLen);
        dest[copyLen] = '\0';
        return dest;
    }

    return NULL;
}


