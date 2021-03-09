/* date = December 21st 2020 7:06 pm */

#ifndef TYPES_H
#define TYPES_H

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

typedef unsigned char uint8;

typedef unsigned short ushort;

typedef unsigned int uint;
//typedef int64_t uint;

typedef float Float;
//typedef double Float;

typedef uint64_t uint64;

typedef struct CharU8{ char x[5]; }CharU8;

#define DEBUG(...) do{ printf("[DEBUG] "); printf(__VA_ARGS__); }while(0)
#define DEBUG_MSG(...) do{ printf("[%s] ", MODULE_NAME); printf(__VA_ARGS__); }while(0)
//#define LEX_DEBUG(...) DEBUG(__VA_ARGS__)
#define LEX_DEBUG(...)

inline void __assert_check(bool v, const char *name, const char *filename,
                           int line, const char *msg)
{
    if(!v){
        if(!msg)
            printf("Assert: %s (%s:%d) : (No message)\n", name, filename, line);
        else
            printf("Assert: %s (%s:%d) : (%s)\n", name, filename, line, msg);
        //exit(0);
        getchar();
    }
}

#define AssertErr(x, msg) __assert_check((x), #x, __FILE__, __LINE__, msg)


#if !defined(DEBUG_BUILD)
#define Assert(x) 
#define AssertA(x, msg)
#else
#define Assert(x) __assert_check((x), #x, __FILE__, __LINE__, NULL)
#define AssertA(x, msg) AssertErr(x, msg)
#endif

#if defined(MEMORY_DEBUG)
#include <string>
#include <iostream>
#include <glad/glad.h>

#define GL_GPU_MEM_INFO_TOTAL_AVAILABLE_MEM_NVX 0x9048
#define GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX 0x9049

#define GL_CHK_BINDING(bufId, size) __gpu_check_binding(bufId, size, __FILE__, __LINE__)

#define GL_CHK(x) do{ __gl_clear_errors(); (x);\
__gl_validate(#x, __LINE__, __FILE__);}while(0)

void __gl_clear_errors();

std::string translateGLError(int errorCode);

void __gl_validate(const char *cmd, int line, const char *fileName);

void __gpu_gl_get_memory_usage();

void __cpu_get_memory_usage(long unsigned int mem);

void __memory_freeze();

void __memory_compare_state();

inline void __gpu_check_binding(int bufferId, long int bindingSize,
                                const char *filename, int line)
{
    GLint size = 0;
    GL_CHK(glBindBuffer(GL_ARRAY_BUFFER, bufferId));
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &size);
    if(size < bindingSize){
        printf("[GPU] Binding of buffer %d will corrupt memory\n", bufferId); 
        printf("[GPU]  **Attempted %ld, max is %d\n", bindingSize, size);
        getchar(); // sigtrap has proven to be unreliable
    }
}


#else
#define GL_CHK(x) (x)
#define GL_CHK_BINDING(bufId, size) do{}while(0)
#endif


//TODO: Make allocator
#define DefaultAllocatorSize 64
#define AllocatorGet(size) _get_memory(size, __FILE__, __LINE__)
#define AllocatorGetN(type, n) (type *)_get_memory(sizeof(type) * n, __FILE__, __LINE__)
#define AllocatorGetDefault(type) (type *)_get_memory(DefaultAllocatorSize * sizeof(type), __FILE__, __LINE__)
#define AllocatorExpand(type, ptr, n, o) (type*)_expand_memory(sizeof(type)*((n)), sizeof(type)*((o)), ptr, __FILE__, __LINE__)
#define AllocatorFree(ptr) _free_memory((void **)&(ptr), __FILE__, __LINE__)

#if defined(MEMORY_DEBUG)
#include <map>
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#define DebugMap std::map<void*,MemoryEntry,bool(*)(void*, void*)>

struct MemoryEntry{
    long int size;
    char filename[256];
    int line;
};

extern DebugMap freezed;
extern DebugMap debug_memory_map;
extern long unsigned int debug_memory_usage;

#endif

inline void _debugger_trace(int sig){
#if defined(MEMORY_DEBUG)
    void *array[10];
    size_t size = 0;
    size = backtrace(array, 10);
    if(sig != 0){
        fprintf(stdout, "Error signal %d:\n", sig);
    }
	__cpu_get_memory_usage(debug_memory_usage);
	__gpu_gl_get_memory_usage();
    backtrace_symbols_fd(array, size, STDOUT_FILENO);
    exit(1);
#else
    (void)sig;
#endif
}

inline void *_get_memory(long size, const char *filename, uint line){
#if defined(MEMORY_DEBUG)
    printf("CALLOC %lu (%s : %d)...", size, filename, line); fflush(stdout);
#endif

    void *ptr = calloc(size, 1);

#if defined(MEMORY_DEBUG)
    MemoryEntry entry = {
        .size = size,
    };
    snprintf(entry.filename, 256, "%s", filename);
    entry.line = line;
    debug_memory_map[ptr] = entry;
    debug_memory_usage += size;
    printf("OK - Inserted %p\n", ptr);
    __cpu_get_memory_usage(debug_memory_usage);
#endif

    if(ptr == NULL){
        printf("Failed to get memory of size: %lx (%s:%u)\n", size, filename, line);
    }
    return ptr;
}

inline void *_expand_memory(long size, long osize, void *p,
                            const char *filename, uint line)
{
#if defined(MEMORY_DEBUG)
    if(p != nullptr){
        printf("REALLOC %lu (%s : %d)...", size, filename, line); fflush(stdout);
    }else{
        printf("REALLOC %lu (%s : %d) of nullptr\n", size, filename, line);
        _debugger_trace(0);
    }
    
    if(debug_memory_map.find(p) != debug_memory_map.end()){
        MemoryEntry e = debug_memory_map[p];
		debug_memory_usage -= e.size;
        debug_memory_map.erase(p);
    }else{
        printf("Realloc of unknown address %p\n", p);
        _debugger_trace(0);
    }
	__cpu_get_memory_usage(debug_memory_usage);
#endif
    
    void *ptr = realloc(p, size);

#if defined(MEMORY_DEBUG)
    MemoryEntry entry = {
        .size = size,
    };
    snprintf(entry.filename, 256, "%s", filename);
    entry.line = line;
    debug_memory_map[ptr] = entry;
    debug_memory_usage += size;
    printf("OK - Inserted %p\n", ptr);
    __cpu_get_memory_usage(debug_memory_usage);
#endif
    
    if(ptr == NULL){
        printf("Failed to expand memory to size: %lx (%s:%u)\n", size, filename, line);
    }else{
        unsigned char*start = (unsigned char *)ptr + osize;
        memset(start, 0x00, size - osize);
    }
    return ptr;
}

inline void _free_memory(void **ptr, const char *filename, uint line){
    if(ptr){
        if(*ptr){
#if defined(MEMORY_DEBUG)
            printf("FREE %p (%s : %d)...", *ptr, filename, line); fflush(stdout);
            if(debug_memory_map.find(*ptr) != debug_memory_map.end()){
                MemoryEntry e = debug_memory_map[*ptr];
				debug_memory_usage -= e.size;
                debug_memory_map.erase(*ptr);
            }else{
                printf("Freeing pointer outside map %p\n", *ptr);
                _debugger_trace(0);
            }
#endif
            
            free(*ptr);
            
#if defined(MEMORY_DEBUG)
            printf("OK\n");
			__cpu_get_memory_usage(debug_memory_usage);
#endif
            *ptr = nullptr;
        }
    }
}

inline void DebuggerRoutines(){
#if defined(MEMORY_DEBUG)
    signal(SIGSEGV, _debugger_trace);
    signal(SIGABRT, _debugger_trace);
#else
    _debugger_trace(0);
#endif
}

#endif //TYPES_H
