#if defined(_WIN32)
#include <dbg.h>

int Dbg_GetFunctions(Dbg *dbg){
    printf("[UNIMPLEMENTED] Attempted to get debugging pointers for windows!\n");
    return 0;
}

void Dbg_Free(Dbg *dbg){
    printf("[UNIMPLEMENTED] Attempted to free debugging pointers for windows!\n");
}
#endif