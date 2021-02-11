#include <undo.h>
#include <types.h>
#include <utilities.h>

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
    Memcpy(&stack->items[stack->top], item, sizeof(BufferChange));
    stack->top = (stack->top + 1) % stack->capacity;
    stack->elements = Min(stack->elements+1, stack->capacity);
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
}

void UndoSystemGetMemory(){
    uSystem.bufferPool = (Buffer **)AllocatorGet(sizeof(Buffer *) * 1024);
    uSystem.head = 0;
    uSystem.size = 1024;
}

Buffer *UndoSystemGetBuffer(){
    if(uSystem.bufferPool == nullptr || uSystem.head >= uSystem.size-1){
        UndoSystemGetMemory();
    }
    uint h = uSystem.head;
    uSystem.head++;
    return uSystem.bufferPool[h];
}