#include <undo.h>
#include <types.h>
#include <utilities.h>
#include <buffers.h>
#include <string.h>

#define MODULE_NAME "UndoRedo"

typedef struct{
    Buffer **bufferPool;
    uint size;
    uint head;
}UndoSystem;

static UndoSystem uSystem = {
    .bufferPool = nullptr,
    .size = 0,
    .head = 0,
};

static void DoStack_ReleaseIfNeeded(BufferChange *item){
    item->buffer = nullptr;
}
static void DoStack_FreeIfNeeded(BufferChange *item){
    if(item->buffer != nullptr){
        UndoSystemTakeBuffer(item->buffer);
        item->buffer = nullptr;
    }
}

DoStack *DoStack_Create(){
    DoStack *stack = (DoStack *)AllocatorGet(sizeof(DoStack));
    AssertA(stack != nullptr, "Failed to get stack memory");
    stack->top = 0;
    stack->capacity = MAX_DO_STACK_SIZE;
    stack->elements = 0;
    return stack;
}

int DoStack_Size(DoStack *stack){
    return stack->elements;
}

int DoStack_IsEmpty(DoStack *stack){
    return stack->elements == 0 ? 1 : 0;
}

void DoStack_Push(DoStack *stack, BufferChange *item){
    uint toFree = stack->top;
    DoStack_FreeIfNeeded(&stack->items[toFree]);
    Memcpy(&stack->items[stack->top], item, sizeof(BufferChange));
    stack->top = (stack->top + 1) % stack->capacity;
    stack->elements = Min(stack->elements+1, stack->capacity);
    //printf("[PUSH]Stack len: %u, freeing %u ( %p )\n", stack->elements, toFree, item->buffer);
}

BufferChange *DoStack_Peek(DoStack *stack){
    if(DoStack_IsEmpty(stack)){
        return nullptr;
    }else{
        uint peek = (stack->top + stack->capacity - 1) % stack->capacity;
        return &stack->items[peek];
    }
}

void DoStack_Pop(DoStack *stack){
    stack->top = (stack->top + stack->capacity - 1) % stack->capacity;
    stack->elements = stack->elements > 0 ? stack->elements-1 : 0;
    DoStack_ReleaseIfNeeded(&stack->items[stack->top]);
    //printf("[POP]Stack len: %u, releasing %u ( %p )\n",stack->elements, stack->top, stack->items[stack->top].buffer);
}

void UndoSystemGetMemory(){
    static long int count = 0;
    uSystem.bufferPool = (Buffer **)AllocatorGet(sizeof(Buffer *) * MAX_DO_STACK_SIZE);
    uSystem.head = 0;
    uSystem.size = MAX_DO_STACK_SIZE;
    for(uint i = 0; i < uSystem.size; i++){
        Buffer *buffer = (Buffer *)AllocatorGet(sizeof(Buffer));
        buffer->size = 0;
        buffer->count = 0;
        buffer->taken = 0;
        buffer->data = nullptr;
        buffer->tokens = nullptr;
        buffer->tokenCount = 0;
        uSystem.bufferPool[i] = buffer;
    }
    count += uSystem.size;
    DEBUG_MSG("Allocated %d more buffers, current: %ld\n",
              MAX_DO_STACK_SIZE, count);
}

void UndoSystemTakeBuffer(Buffer *buffer){
    if(uSystem.head > 0){
        uint h = uSystem.head-1;
        uSystem.bufferPool[h] = buffer;
        uSystem.head--;
        DEBUG_MSG("Grabbed buffer, head: %u, size: %u\n", uSystem.head, uSystem.size);
    }else{ // cannot hold this buffer, free it
        Buffer_Free(buffer);
        AllocatorFree(buffer);
    }
}

Buffer *UndoSystemGetBuffer(){
    if(uSystem.bufferPool == nullptr || uSystem.head >= uSystem.size-1){
        UndoSystemGetMemory();
    }
    uint h = uSystem.head;
    uSystem.head++;
    return uSystem.bufferPool[h];
}

void _UndoRedoPushMerge(UndoRedo *redo, Buffer *buffer, vec2ui cursor, int is_undo){
    BufferChange bufferChange = {
        .bufferInfo = cursor,
        .buffer = nullptr,
        .text = nullptr,
        .size = 0,
        .change = CHANGE_MERGE,
    };
    
    Buffer *b = UndoSystemGetBuffer();
    if(b == nullptr){
        BUG();
        printf("Undo system retrieved nullptr\n");
        return;
    }
    
    Buffer_CopyDeep(b, buffer);
    bufferChange.buffer = b;
    
    if(is_undo)
        DoStack_Push(redo->undoStack, &bufferChange);
    else
        DoStack_Push(redo->redoStack, &bufferChange);
}

void _UndoRedoUndoPushNewLine(UndoRedo *redo, vec2ui baseU8, int is_undo){
    BufferChange bufferChange = {
        .bufferInfo = baseU8,
        .buffer = nullptr,
        .text = nullptr,
        .size = 0,
        .change = CHANGE_NEWLINE,
    };
    if(is_undo)
        DoStack_Push(redo->undoStack, &bufferChange);
    else
        DoStack_Push(redo->redoStack, &bufferChange);
}

void UndoRedoUndoPushInsert(UndoRedo *redo, Buffer *buffer, vec2ui cursor){
    BufferChange bufferChange = {
        .bufferInfo = cursor,
        .buffer = nullptr,
        .text = nullptr,
        .size = 0,
        .change = CHANGE_INSERT,
    };
    
    Buffer *b = UndoSystemGetBuffer();
    Buffer_CopyDeep(b, buffer);
    bufferChange.buffer = b;
    DoStack_Push(redo->undoStack, &bufferChange);
}

void UndoRedoUndoPushRemove(UndoRedo *redo, Buffer *buffer, vec2ui cursor){
    BufferChange bufferChange = {
        .bufferInfo = cursor,
        .buffer = buffer,
        .text = nullptr,
        .size = 0,
        .change = CHANGE_REMOVE,
    };
    
    Buffer *b = UndoSystemGetBuffer();
    Buffer_CopyDeep(b, buffer);
    bufferChange.buffer = b;
    DoStack_Push(redo->undoStack, &bufferChange);
}

void UndoRedoUndoPushRemoveBlock(UndoRedo *redo, vec2ui start, vec2ui end){
    BufferChange bufferChange = {
        .bufferInfo = start,
        .bufferInfoEnd = end,
        .buffer = nullptr,
        .text = nullptr,
        .size = 0,
        .change = CHANGE_BLOCK_REMOVE,
    };
    
    DoStack_Push(redo->undoStack, &bufferChange);
}

void UndoRedoUndoPushInsertBlock(UndoRedo *redo, vec2ui start, char *text, uint size){
    BufferChange bufferChange = {
        .bufferInfo = start,
        .buffer = nullptr,
        .text = StringDup(text, size),
        .size = size,
        .change = CHANGE_BLOCK_INSERT,
    };
    
    DoStack_Push(redo->undoStack, &bufferChange);
}

void UndoRedoUndoPushNewLine(UndoRedo *redo, vec2ui baseU8){
    _UndoRedoUndoPushNewLine(redo, baseU8, 1);
}

void UndoRedoRedoPushNewLine(UndoRedo *redo, vec2ui baseU8){
    _UndoRedoUndoPushNewLine(redo, baseU8, 0);
}

void UndoRedoUndoPushMerge(UndoRedo *redo, Buffer *buffer, vec2ui cursor){
    _UndoRedoPushMerge(redo, buffer, cursor, 1);
}

void UndoRedoRedoPushMerge(UndoRedo *redo, Buffer *buffer, vec2ui cursor){
    _UndoRedoPushMerge(redo, buffer, cursor, 0);
}

void UndoRedoPopUndo(UndoRedo *redo){
    DoStack_Pop(redo->undoStack);
}

void UndoRedoPopRedo(UndoRedo *redo){
    DoStack_Pop(redo->redoStack);
}

BufferChange *UndoRedoGetNextRedo(UndoRedo *redo){
    return DoStack_Peek(redo->redoStack);
}

BufferChange *UndoRedoGetNextUndo(UndoRedo *redo){
    BufferChange *bChange = DoStack_Peek(redo->undoStack);
    if(bChange){
        DEBUG_MSG("[UNDO] Undo for %s - Stack size: %u\n",
                  UndoRedo_GetStringID(bChange->change), redo->undoStack->elements);
    }
    return bChange;
}

void UndoRedoCleanup(UndoRedo *redo){
    BufferChange *bChange = UndoRedoGetNextUndo(redo);
    while(bChange != nullptr){
        DoStack_FreeIfNeeded(bChange);
        UndoRedoPopUndo(redo);
        bChange = UndoRedoGetNextUndo(redo);
    }
    
    bChange = UndoRedoGetNextRedo(redo);
    while(bChange != nullptr){
        DoStack_FreeIfNeeded(bChange);
        UndoRedoPopRedo(redo);
        bChange = UndoRedoGetNextRedo(redo);
    }
}