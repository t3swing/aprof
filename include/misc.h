#ifndef __MISC_H__
#define __MISC_H__
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "deps.h"

typedef void * ARRAY_PS;

/**
* a simple array for symbol storage
* @membSize: sizeof(member)
* @step: auto expand by this step
* @return:a struct ARRAY_S * type pointer
*/
ARRAY_PS arrayNew(int membSize,int step);
int arrayExpandTo(ARRAY_PS array,int size);
void * arrayAdd(ARRAY_PS array,void * element);
void * arrayMutable(ARRAY_PS array,int * nMemb);
int arrayDel(ARRAY_PS array);

/* a salf strcpy copy like strlcpy */
char *sstrcpy(char *dest, const char *src,int dsize);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __MISC_H__ */

