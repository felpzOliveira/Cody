/* date = December 21st 2020 7:06 pm */

#ifndef MAYHEM_TYPES_H
#define MAYHEM_TYPES_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

typedef unsigned int uint;
//typedef int64_t uint;

typedef float Float;
//typedef double Float;

#define DEBUG(...) do{ printf("[DEBUG] "); printf(__VA_ARGS__); }while(0)
#define DEBUG_MSG(...) do{ printf("[%s] ", MODULE_NAME); printf(__VA_ARGS__); }while(0)
//#define LEX_DEBUG(...) DEBUG(__VA_ARGS__)
#define LEX_DEBUG(...)

#define Assert(x) __assert_check((x), #x, __FILE__, __LINE__, NULL)
#define AssertA(x, msg) __assert_check((x), #x, __FILE__, __LINE__, msg)

inline void __assert_check(bool v, const char *name, const char *filename,
                           int line, const char *msg)
{
    if(!v){
        if(!msg)
            printf("Assert: %s (%s:%d) : (No message)\n", name, filename, line);
        else
            printf("Assert: %s (%s:%d) : (%s)\n", name, filename, line, msg);
        exit(0);
    }
}

//TODO: Make allocator
#define DefaultAllocatorSize 64
#define AllocatorGet(size) calloc(size, 1)
#define AllocatorGetDefault(type) (type *)calloc(DefaultAllocatorSize, sizeof(type))
#define AllocatorExpand(type, ptr, n) (type*)realloc(ptr,sizeof(type)*((n)))
#define AllocatorFree(ptr) free(ptr)

#endif //MAYHEM_TYPES_H
