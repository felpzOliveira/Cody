/* date = January 10th 2021 11:58 am */

#ifndef UTILITIES_H
#define UTILITIES_H
#include <types.h>
#include <geometry.h>
#include <string>
#include <fstream>
#include <signal.h>
#include <limits.h>
#include <unistd.h>
#include <functional>
#include <sys/stat.h>
#include <vector>

#define MAX_STACK_SIZE 1024
#define MAX_BOUNDED_STACK_SIZE 16
#define DebugBreak() raise(SIGTRAP)
#define MAX_DESCRIPTOR_LENGTH 256
#define CHDIR(x) chdir(x)
#define IGNORE(x) (void)!(x)

#define BUG() printf("=========== BUG ===========\nLine:%d\n", __LINE__)
#define NullRet(x) if(!(x)) return

const Float kInv255 = 0.003921569;
const Float kAlphaReduceDefault = 1.0;
const Float kAlphaReduceInactive = 0.6;
const int kMaximumIndentEmptySearch = 9;
const Float kTransitionScrollInterval = 0.15;
const Float kTransitionJumpInterval = 0.1;
const Float kViewSelectableListScaling = 2.0;
const Float kAutoCompleteListScaling = 1.2;
const Float kAutoCompleteMaxHeightFraction = 0.35;
const Float kViewSelectableListOffset = 0.05;
const Float kTransitionControlIndices = 1.5;
const Float kTransitionCopyFadeIn = 0.28;
const Float kOnes[] = {1,1,1,1};

typedef enum{
    DescriptorFile = 0,
    DescriptorDirectory,
}FileType;

struct String{
    char *data;
    uint size;
};

/* Representation of a file, path holds the file name with regards to its base directory */
struct FileEntry{
    char path[MAX_DESCRIPTOR_LENGTH];
    uint pLen;
    FileType type;
    int isLoaded;
};

/* Reads a file and return its content in a new pointer */
char *GetFileContents(const char *path, uint *size);

/*
* Get the right-most value of a path string inside the path given.
* In case no one is found it returns -1 otherwise the right-most position located.
*/
int GetRightmostSplitter(const char *path, uint size);

/*
* Gets the position of the character where the extension of the file is defined
*/
int GetFilePathExtension(const char *path, uint size);

/*
* Utility for getting a line from a opened std::istream.
*/
std::istream &GetNextLineFromStream(std::istream &is, std::string &str);

/*
* Simply utility for removing unwanted characters from a line '\n' and '\r' at the
* end only, it is a line after all.
*/
void RemoveUnwantedLineTerminators(std::string &line);

/*
* Checks if the character given is alphanumeric.
*/
int TerminatorChar(char v);

/*
* Checks if strings are equal.
*/
int StringEqual(char *s0, char *s1, uint maxn);

/*
* Duplicates a string.
*/
char *StringDup(char *s0, uint len);
String *StringMake(char *s0, uint len);

/*
* Checks if a string contains only digits.
*/
int StringIsDigits(char *s0, uint len);

/*
* Converts a string into a unsigned integer, clamping if required.
*/
uint StringToUnsigned(char *s0, uint len);

/*
* Checks if a given string 's0' starts with another string 's1'.
*/
int StringStartsWith(char *s0, uint s0len, char *s1, uint s1len);

/*
* Return address of first non-empty character in string, returns -1
* in case of failures.
*/
int StringFirstNonEmpty(char *s0, uint s0len);

/*
* Return an accessor to the begining of the next work in the string 's0'.
* In case there is no other word it returns nullptr. Returns the size of
* the word in the output 'size', in case a word is found.
*/
char *StringNextWord(char *s0, uint s0len, uint *size);

/*
* Split a string into a list of string, splits based on space.
*/
void StringSplit(std::string s0, std::vector<std::string> &splitted);

/*
* Find the first ocurrency of 'v' inside 's0'.
*/
int StringFindFirstChar(char *s0, uint s0len, char v);

/*
* Find the last ocurrency of 'v' inside 's0'.
*/
int StringFindLastChar(char *s0, uint s0len, char v);

/*
* Get the n-th digit of value. Digits are count backwards, i.e.:
* the number '1234567' will give '7' for GetDigitOf(1234567, 0) in
* order to avoid stacking numbers and perform multiple loops.
*/
uint GetDigitOf(uint value, uint n);

/*
* Generates a list of the files and directories present in 'basePath',
* return >= 0 in case of success, -1 in case it could not read 'basePath'.
* Warning: In case *entries != nullptr this function might realloc its pointer
* to fit all files present in 'basePath'. The number of elements discovered
* is returned in 'n' while 'size' records the length of the FileEntry list
* at the end of the procedure.
*/
int ListFileEntries(char *basePath, FileEntry **entries, uint *n, uint *size);

/*
* Attempt to guess what a path means, and fill a entry with the description.
* Returns -1 in case it could not guess, 0 otherwise. It writes the full folder
* path in 'folder' which should be of length PATH_MAX.
*/
int GuessFileEntry(char *path, uint size, FileEntry *entry, char *folder);

/*
* Quickly check if a file exists.
*/
int FileExists(char *path);

/*
* Concatenate the path + folder into a string if the path does not look like
* a full path. If it does return the original path.
*/
std::string ExpandFilePath(char *path, uint size, char *folder);

/*
* Gets the current working directory, 'dir' should be of size 'len' = PATH_MAX.
*/
void GetCurrentWorkingDirectory(char *dir, uint len);

/*
* Find the first ocurrency of s0 into s1. Returns -1 in case no one is found.
* (Brute-force)
*/
int StringSearch(char *s0, char *s1, uint s0len, uint s1len);

/*
* Find the last ocurrency of s0 into s1. Returns -1 in case no one is found.
* (Brute-force)
*/
int ReverseStringSearch(char *s0, char *s1, uint s0len, uint s1len);

/*
* Perform an accelerated string search algorithm looking for string s0 inside s1.
* Warning: This is currently using a 255 table size, it will not match all UTF-8 chars.
*/
//TODO: UTF-8
void FastStringSearchInit(char *s0, uint s0len);
int  FastStringSearch(char *s0, char *s1, uint s0len, uint s1len);

/*
* Gets the name path of the file without the full path.
*/
uint GetSimplifiedPathName(char *fullPath, uint len);

/*
* Get the size and content of the of a UTF-8 character at a given position.
* The string 's0' does not need to be null terminated in which case 'len' should
* hold its size. In case 'len' = -1 this routine uses standard methods to detect
* the size of the string 's0'.
*/
uint StringComputeCharU8At(char *s0, CharU8 *chr, uint at, int len=-1);

/*
* Computes the amount of UTF-8 characters inside the string 's0'.
*/
uint StringComputeU8Count(char *s0, uint len);

/*
* Convert colors.
*/
vec4i ColorFromHex(uint hex);
vec4f ColorFromHexf(uint hex);
vec4i ColorFromRGBA(vec4f color);
vec4f ColorFromInt(vec4i col);

/*
* Count the amount of digits in a int.
*/
uint DigitCount(uint value);

/*
* Convert a codepoint to sequence of chars, i.e.: string,
* chars are written to 'c' and 'c' must be a vector with minimum size = 5.
* Returns the amount of bytes written into 'c'.
*/
int CodepointToString(int cp, char *c);

/*
* Gets the first codepoint in the string given by 'u' with size 'size'
* and return its integer value, returns the number of bytes taken to generate
* the codepoint in 'off'.
*/
int StringToCodepoint(char *u, int size, int *off);

/*
* Checks if a string contains a extensions, mostly for GLX/GL stuff
*/
int ExtensionStringContains(const char *string, const char *extensions);

template<typename Fn>
inline double MeasureInterval(const Fn &fn){
    clock_t start = clock();
    fn();
    return double(clock() - start)/CLOCKS_PER_SEC;
}

/* memcpy */
void Memcpy(void *dst, void *src, uint size);
/* memset */
void Memset(void *dst, unsigned char v, uint size);

#if 0
    #define MeasureTime(msg, ...) do{\
        clock_t _beg_clk = clock();\
        __VA_ARGS__\
        double _clk_interval = double(clock() - _beg_clk)/CLOCKS_PER_SEC;\
        printf("[PERF] %s took %g ms\n", msg, _clk_interval * 1000.0);\
    }while(0)
#else
    #define MeasureTime(msg, ...) __VA_ARGS__
#endif
/* Data structures - Structures create their own copy of the item so you can free yours*/

/*
* Doubly linked list.
*/
template<typename T>
struct ListNode{
    T *item;
    struct ListNode<T> *prev;
    struct ListNode<T> *next;
};

template<typename T>
struct List{
    ListNode<T> *head;
    ListNode<T> *tail;
    uint size;
};

template<typename T> inline uint List_FetchAsArray(List<T> *list, T **array,
                                                   uint arraySize, uint start=0)
{
    if(start >= list->size) return 0;
    uint count = Min(list->size - start, arraySize);
    ListNode<T> *ptr = list->head;
    uint s = 0;
    for(uint i = 0; i < start && ptr; i++){
        ptr = ptr->next;
    }

    for(uint i = 0; i < count && ptr; i++){
        array[s++] = ptr->item;
        ptr = ptr->next;
    }

    return s;
}

template<typename T> inline List<T> *List_Create(){
    List<T> *list = AllocatorGetN(List<T>, 1);
    AssertA(list != nullptr, "Failed to get list memory");
    list->size = 0;
    list->head = nullptr;
    list->tail = nullptr;
    return list;
}

template<typename T> inline
T *List_Find(List<T> *list, std::function<int(T *)> call){
    if(!list) return nullptr;

    ListNode<T> *ax = list->head;
    int found = 0;
    while(ax != nullptr && found == 0){
        found = call(ax->item);
        if(found == 0){
            ax = ax->next;
        }
    }

    return ax ? ax->item : nullptr;
}

template<typename T> inline
void List_Transverse(List<T> *list, std::function<int(T *)> transverse){
    if(!list) return;
    ListNode<T> *ax = list->head;
    while(ax != nullptr){
        if(transverse(ax->item) == 0){
            break;
        }

        ax = ax->next;
    }
}

template<typename T> inline
void List_Erase(List<T> *list, std::function<int(T *)> finder){
    if(!list) return;
    ListNode<T> *ax = list->head;
    int ht = list->head == list->tail ? 1 : 0;
    while(ax != nullptr){
        if(finder(ax->item)){
            if(ax == list->head){
                list->head = ax->next;
                if(ht) list->tail = list->head;
            }

            if(ax == list->tail && !ht){
                list->tail = ax->prev;
            }

            if(ax->next != nullptr){
                ax->next->prev = ax->prev;
            }

            if(ax->prev != nullptr){
                ax->prev->next = ax->next;
            }

            AllocatorFree(ax->item);
            AllocatorFree(ax);
            list->size--;
            break;
        }

        ax = ax->next;
    }
}

template<typename T> inline void List_Push(List<T> *list, T *item){
    ListNode<T> *node = AllocatorGetN(ListNode<T>, 1);
    AssertA(node != nullptr, "Failed to get node memory");
    node->item = AllocatorGetN(T, 1);
    Memcpy(node->item, item, sizeof(T));
    node->prev = nullptr;
    node->next = nullptr;

    if(list->size == 0){
        list->head = node;
        list->tail = node;
    }else{
        list->tail->next = node;
        node->prev = list->tail;
        list->tail = node;
    }

    list->tail = node;
    list->size++;
}

template<typename T> inline void List_Pop(List<T> *list){
    if(list->size > 0 && list->head){
        if(list->size == 1){
            AllocatorFree(list->head->item);
            AllocatorFree(list->head);
            list->tail = nullptr;
            list->head = nullptr;
        }else{
            ListNode<T> *l = list->head;
            list->head = list->head->next;
            AllocatorFree(l->item);
            AllocatorFree(l);
        }

        list->size--;
    }
}

template<typename T> inline void List_Clear(List<T> *list){
    while(list->size > 0){
        List_Pop<T>(list);
    }
}

template<typename T> inline void List_Top(List<T> *list, T *item){
    if(item && list){
        item = list->head;
    }
}

/*
* Priority queue. Implemented as a heap.
* comparator function is of the form:
*    int comp(Item *a, Item *b){
*        return *a > *b;
*    }
* i.e.: should return true in case a is bigger than b.
*/
template<typename Item>
struct PriorityQueueItem{
    Item *data;
};

template<typename Item>
struct PriorityQueue{
    std::function<int(Item *, Item *)> comparator;
    PriorityQueueItem<Item> *entries;
    uint size;
    uint length;
    uint growsize;
};

template<typename Item> inline
PriorityQueue<Item> *PriorityQueue_Create(std::function<int(Item*,Item*)> fn,uint gsize=5){
    PriorityQueue<Item> *pq = (PriorityQueue<Item> *)
                                AllocatorGet(sizeof(PriorityQueue<Item>));
    AssertA(pq != nullptr, "Failed to get priority queue memory");
    pq->growsize = Max(1, gsize);
    pq->size = 0;
    pq->length = pq->growsize;
    pq->comparator = fn;

    pq->entries = AllocatorGetN(PriorityQueueItem<Item>, pq->growsize);
    AssertA(pq->entries != nullptr, "Failed to get priority queue entries memory");
    return pq;
}

template<typename Item> inline
void PriorityQueue_Heapify(PriorityQueue<Item> *pq, int i){
    int largest = i;
    uint left  = 2 * i + 1;
    uint right = 2 * i + 2;
    if(left < pq->size){
        if(pq->comparator(pq->entries[left].data,
                          pq->entries[largest].data))
        {
            largest = left;
        }
    }

    if(right < pq->size){
        if(pq->comparator(pq->entries[right].data,
                          pq->entries[largest].data))
        {
            largest = right;
        }
    }

    if(largest != i){
        Item *idata = pq->entries[i].data;
        pq->entries[i].data = pq->entries[largest].data;
        pq->entries[largest].data = idata;
        PriorityQueue_Heapify(pq, largest);
    }
}

template<typename Item> inline
int PriorityQueue_Push(PriorityQueue<Item> *pq, Item *item){
    if(!pq || !item) return 0;

    if(!(pq->size < pq->length)){
        //expand array
        pq->entries = AllocatorExpand(PriorityQueueItem<Item>, pq->entries,
                                      pq->length + pq->growsize, pq->length);
        pq->length += pq->growsize;
    }

    // push new entry into last position
    pq->entries[pq->size].data = item;
    pq->size += 1;
    if(pq->size > 1){
        // heapify the tree
        for(int i = pq->size / 2 - 1; i >= 0; i--){
            PriorityQueue_Heapify(pq, i);
        }
    }
    return 1;
}

template<typename Item> inline
void PriorityQueue_Pop(PriorityQueue<Item> *pq){
    if(pq->size > 0){
        Item *item = pq->entries[0].data;
        pq->entries[0].data = pq->entries[pq->size-1].data;
        pq->entries[pq->size-1].data = item;
        pq->size -= 1;

        if(pq->size > 1){
            // heapify the tree
            for(int i = pq->size / 2 - 1; i >= 0; i--){
                PriorityQueue_Heapify(pq, i);
            }
        }
    }
}

template<typename Item> inline
uint PriorityQueue_Size(PriorityQueue<Item> *pq){
    return pq->size;
}

template<typename Item> inline
Item *PriorityQueue_Peek(PriorityQueue<Item> *pq){
    if(pq->size == 0) return nullptr;
    return pq->entries[0].data;
}

template<typename Item> inline
void PriorityQueue_Clear(PriorityQueue<Item> *pq){
    pq->size = 0;
}

template<typename Item> inline
void PriorityQueue_Free(PriorityQueue<Item> *pq){
    if(pq->entries){
        AllocatorFree(pq->entries);
        pq->size = 0;
    }
    AllocatorFree(pq);
}

/*
* Fixed length stack.
*/
template<typename T>
struct FixedStack{
    T items[MAX_STACK_SIZE];
    int capacity;
    int top;
};

template<typename T> inline FixedStack<T> *FixedStack_Create(){
    FixedStack<T> *stack = (FixedStack<T> *)AllocatorGet(sizeof(FixedStack<T>));
    AssertA(stack != nullptr, "Failed to get stack memory");
    stack->top = -1;
    stack->capacity = MAX_STACK_SIZE;
    return stack;
}

template<typename T> inline int FixedStack_IsFull(FixedStack<T> *stack){
    return stack->top == stack->capacity - 1;
}

template<typename T> inline int FixedStack_Size(FixedStack<T> *stack){
    return stack->top + 1;
}

template<typename T> inline int FixedStack_IsEmpty(FixedStack<T> *stack){
    return stack->top == -1;
}

template<typename T> inline void FixedStack_Push(FixedStack<T> *stack, T *item){
    AssertA(!FixedStack_IsFull(stack), "Stack is full");
    Memcpy(&stack->items[++stack->top], item, sizeof(T));
}

template<typename T> inline T *FixedStack_Peek(FixedStack<T> *stack){
    if(FixedStack_IsEmpty(stack)){
        return nullptr;
    }else{
        return &stack->items[stack->top];
    }
}

template<typename T> inline void FixedStack_Pop(FixedStack<T> *stack){
    if(!FixedStack_IsEmpty(stack)){
        stack->top--;
    }
}

#endif //UTILITIES_H
