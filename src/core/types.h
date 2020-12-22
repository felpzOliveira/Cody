/* date = December 21st 2020 7:06 pm */

#ifndef MAYHEM_TYPES_H
#define MAYHEM_TYPES_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

typedef unsigned int uint;
//typedef int64_t uint;

#define Assert(exp, msg) if(!(exp)){\
printf("Assert Failure: %s (%s) %s:%d\n", #exp, msg, __FILE__, __LINE__);\
exit(0);\
}

//TODO: Make allocator
#define DefaultAllocatorSize 64
#define AllocatorGet(size) calloc(size, 1)
#define AllocatorGetDefault(type) (type *)calloc(DefaultAllocatorSize, sizeof(type))
#define AllocatorExpand(type, ptr, n) (type*)realloc(ptr,sizeof(type)*((n)))
#define AllocatorFree(ptr) free(ptr)

#endif //MAYHEM_TYPES_H
