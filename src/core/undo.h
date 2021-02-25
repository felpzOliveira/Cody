/* date = February 11th 2021 2:35 pm */

#ifndef UNDO_H
#define UNDO_H

#define MAX_DO_STACK_SIZE 2048
#include <types.h>
#include <geometry.h>

struct Buffer;

#define BUFFER_CHANGE_INITIALIZER {.bufferInfo = vec2ui(0,0), .buffer = nullptr, .change = CHANGE_NONE}

#define OPERATION_INSERT_CHAR 0
#define OPERATION_REMOVE_CHAR 1
#define OPERATION_INSERT_BLOCK 2
#define OPERATION_REMOVE_BLOCK 3


typedef enum{
    CHANGE_NONE,
    CHANGE_INSERT,
    CHANGE_REMOVE,
    CHANGE_NEWLINE,
    CHANGE_MERGE,
    CHANGE_BLOCK_REMOVE,
    CHANGE_BLOCK_INSERT,
}ChangeID;

typedef struct{
    vec2ui bufferInfo;
    vec2ui bufferInfoEnd;
    Buffer *buffer;
    char *text;
    uint size;
    ChangeID change;
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

inline const char *UndoRedo_GetStringID(ChangeID id){
#define STR_CASE(x) case x : return #x
    switch(id){
        STR_CASE(CHANGE_NONE);
        STR_CASE(CHANGE_INSERT);
        STR_CASE(CHANGE_REMOVE);
        STR_CASE(CHANGE_NEWLINE);
        STR_CASE(CHANGE_MERGE);
        STR_CASE(CHANGE_BLOCK_REMOVE);
        STR_CASE(CHANGE_BLOCK_INSERT);
        default:return "Unknown";
    }
#undef STR_CASE
}

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

/*
* Gets a new buffer.
*/
Buffer *UndoSystemGetBuffer();

void UndoSystemTakeBuffer(Buffer *buffer);

/*
* Pushes into the stack a command to destroy/create a merge.
*/
void UndoRedoUndoPushMerge(UndoRedo *redo, vec2ui baseU8);
void UndoRedoRedoPushMerge(UndoRedo *redo, vec2ui baseU8);

/*
 * Pushes into the stack a command to create/destroy new lines.
*/
void UndoRedoUndoPushNewLine(UndoRedo *redo, vec2ui baseU8);
void UndoRedoRedoPushNewLine(UndoRedo *redo, vec2ui baseU8);

/*
* Pushes into the stack a command to erase/insert text at a buffer.
*/
void UndoRedoUndoPushInsert(UndoRedo *redo, Buffer *buffer, vec2ui cursor);
void UndoRedoUndoPushRemove(UndoRedo *redo, Buffer *buffer, vec2ui cursor);

/*
* Pushes into the stack a comand to remove a block of text pasted.
*/
void UndoRedoUndoPushRemoveBlock(UndoRedo *redo, vec2ui start, vec2ui end);

/*
* Pushes into the stack a command to insert a block of text removed.
*/
void UndoRedoUndoPushInsertBlock(UndoRedo *redo, vec2ui start, char *text, uint size);

/*
* Moves one entry of the undo stack to the redo stack.
*/
void UndoRedoPopUndo(UndoRedo *redo);

/*
* Pops the next element in the redo stack
*/
void UndoRedoPopRedo(UndoRedo *redo);

/*
* Gets the next undo operation (peek).
*/
BufferChange *UndoRedoGetNextUndo(UndoRedo *redo);

/*
* Gets the next redo operation (peek).
*/
BufferChange *UndoRedoGetNextRedo(UndoRedo *redo);

#endif //UNDO_H
