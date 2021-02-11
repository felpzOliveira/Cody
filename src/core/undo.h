/* date = February 11th 2021 2:35 pm */

#ifndef UNDO_H
#define UNDO_H

#define MAX_DO_STACK_SIZE 2048
#include <types.h>
#include <geometry.h>

struct Buffer;

typedef enum{
    CHANGE_INSERT,
    CHANGE_REMOVE,
}ChangeID;

typedef struct{
    uint bufferId;
    Buffer *buffer;
}BufferChange;

/* Circular stack */
typedef struct{
    BufferChange items[MAX_DO_STACK_SIZE];
    uint capacity;
    uint top;
    uint elements;
}DoStack;

typedef struct{
    DoStack *undoStack;
    DoStack *redoStack;
}UndoRedo;

/*
* Creates a new do stack.
*/
DoStack *DoStack_Create();

/*
* Pops the top most element of the stack, if any.
*/
void DoStack_Pop(DoStack *stack);

/*
* Retrieves the size of the do stack.
*/
int DoStack_Size(DoStack *stack);

/*
* Checks if the do stack is empty.
*/
int DoStack_IsEmpty(DoStack *stack);

/*
* Pushes a new item into the do stack.
*/
void DoStack_Push(DoStack *stack, BufferChange *item);

/*
* Gets the top most element of the do stack.
*/
BufferChange *DoStack_Peek(DoStack *stack);

Buffer *UndoSystemGetBuffer();

#endif //UNDO_H
