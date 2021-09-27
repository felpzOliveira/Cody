#include <types.h>

#if defined(MEMORY_DEBUG)
#include <vector>
#include <iterator>

bool cmpr(void* a, void*b) { return (intptr_t)a < (intptr_t)b; }

DebugMap freezed(&cmpr);
DebugMap debug_memory_map(&cmpr);
long unsigned int debug_memory_usage = 0;
std::mutex debug_memory_mutex;

void __memory_freeze(){
    freezed.clear();
    freezed.insert(debug_memory_map.begin(), debug_memory_map.end());
}

void __memory_compare_state(){
    DebugMap::iterator it;
    long unsigned int unused = 0;
    //long unsigned int freezedmem = 0;
    //long unsigned int currentmem = 0;
    for(it = debug_memory_map.begin(); it != debug_memory_map.end(); it++){
        void *addr = it->first;
        MemoryEntry entry = it->second;

        if(freezed.find(addr) == freezed.end()){
            unused += entry.size;
            printf("**** %p %s : %d ( %lu )\n", addr, entry.filename, entry.line, entry.size);
        }
    }

    printf("[MEM] Unused: %lu\n", unused);
}

void __gl_clear_errors(){
    while(glGetError()){
        ;
    }
}

std::string translateGLError(int errorCode){
    std::string error;
    switch (errorCode){
        case GL_INVALID_ENUM:                  error = "INVALID_ENUM"; break;
        case GL_INVALID_VALUE:                 error = "INVALID_VALUE"; break;
        case GL_INVALID_OPERATION:             error = "INVALID_OPERATION"; break;
        case GL_STACK_OVERFLOW:                error = "STACK_OVERFLOW"; break;
        case GL_STACK_UNDERFLOW:               error = "STACK_UNDERFLOW"; break;
        case GL_OUT_OF_MEMORY:                 error = "OUT_OF_MEMORY"; break;
        case GL_INVALID_FRAMEBUFFER_OPERATION: error = "INVALID_FRAMEBUFFER_OPERATION"; break;
        default: error = "Unknown Error";
    }
    return error;
}

void __gl_validate(const char *cmd, int line, const char *fileName){
    int val = glGetError();
    if(val != GL_NO_ERROR){
        std::string msg = translateGLError(val);
        std::cout << "(" << fileName << " : " << line << ") " << cmd << " => " << msg.c_str()
                   << "[" << val << "]" << std::endl;
        Assert(false);
        //exit(0);
    }
}

void __gpu_gl_get_memory_usage(){
    int total_mem_kb = 0;
    glGetIntegerv(GL_GPU_MEM_INFO_TOTAL_AVAILABLE_MEM_NVX, &total_mem_kb);

    int cur_avail_mem_kb = 0;
    glGetIntegerv(GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX, &cur_avail_mem_kb);
    total_mem_kb /= 1024;
    cur_avail_mem_kb /= 1024;
    printf("[GPU] Current Available: %d / %d (MB)\n", cur_avail_mem_kb, total_mem_kb);
}

void __cpu_get_memory_usage(long unsigned int mem){
    double g = (double)mem / 1024.;
    g /= 1024.0;
    MEMORY_PRINT("[CPU] Current size: %g (MB)\n", g);
}

#endif
