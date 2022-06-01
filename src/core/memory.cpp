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

void __cpu_get_memory_usage(long unsigned int mem){
    double g = (double)mem / 1024.;
    g /= 1024.0;
    MEMORY_PRINT("[CPU] Current size: %g (MB)\n", g);
}

#endif
