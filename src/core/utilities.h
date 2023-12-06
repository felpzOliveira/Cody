/* date = January 10th 2021 11:58 am */
#pragma once
#include <types.h>
#include <geometry.h>
#include <string>
#include <fstream>
#include <signal.h>
#include <limits.h>
#include <functional>
#include <sys/stat.h>
#include <vector>

#if defined(_WIN32)
    #include <Windows.h>
    #include <direct.h>
    #include <stdlib.h>
    #define PATH_MAX 256
    #define CHDIR(x) _chdir(x)
    #define MKDIR(x) _mkdir(x)
    #define SEPARATOR_CHAR '\\'
    #define SEPARATOR_STRING "\\"
    #define SLEEP(n) Sleep(n)
#else
    #include <unistd.h>
    #define CHDIR(x) chdir(x)
    #define MKDIR(x) mkdir(x, 0777)
    #define SEPARATOR_CHAR '/'
    #define SEPARATOR_STRING "/"
    #define SLEEP(n) sleep(n)
#endif

#define MAX_STACK_SIZE 1024
#define MAX_BOUNDED_STACK_SIZE 16
#define MAX_DESCRIPTOR_LENGTH 256
#define IGNORE(x) (void)!(x)

/*
* NOTE: Make sure the build in fact supports binary files before
* attempting to set this to zero.
*/
#define REJECT_BINARY_FILES 1

//#define BUG_HUNT

/*
* Utilities for tracing errors and bugs. A lot of BUG() calls are not really bugs
* but states that were not considered. It seems that they happen sometimes but
* are not really issues as the editor recovers from them. However if we ever
* decide we want to extinguish them we can trace by defining BUG_HUNT.
*/
#if defined(BUG_HUNT)
    #define BUG() printf("\n=========== BUG ===========\nLocation %s:%d Func: %s\n",\
                        __FILE__, __LINE__, __func__)
    #define BUG_PRINT(...) printf(__VA_ARGS__)
#else
    #define BUG()do{}while(0)
    #define BUG_PRINT(...)do{}while(0)
#endif
#define NullRet(x) if(!(x)) return

/*
* Some constants that are used through the code.
*/
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
const double kMaximumDecodeRatio = 0.05;

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

/* Changes a string to make sure that it contains the current path delimiter of the system */
void SwapPathDelimiter(std::string &path);

/* Wrapper for mkdir */
int Mkdir(const char *path);

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
* Checks if the character given is a stop point, i.e.:
* ' ', '+', '-', '.', '=', ...
*/
int StopChar(char v);

/*
* Checks if strings are equal.
*/
int StringEqual(char *s0, char *s1, uint maxn);

std::string StringReplace(std::string str, const std::string& from,
                          const std::string& to);

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
* NOTE: For some unknown reason std::stof and std::stod are not working
* whenever we split a command it keeps returning 1 for 1.5, it is breaking
* conversion it might be because it wants a ',' instead of a '.' which is absurd
* so we'll implement our own.
* Converts a string to float.
*/
bool StringToFloat(char *s, char *s_end, Float *result);

/*
* Checks if the string starts with an integer as a first word.
*/
bool StringStartsWithInteger(std::string str);

/*
* Split a string into a list of string, splits are based 'value'.
*/
void StringSplit(std::string s0, std::vector<std::string> &splitted, char value=' ');

/*
* Trims a string.
*/
std::string StringTrim(std::string s);

/*
* Returns the following line, if any, inside the given string.
*/
std::string FetchSecondLine(std::string s);

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
* Generates a list of the files and directories present in 'basePath',
* return >= 0 in case of success, -1 in case it could not read 'basePath'.
* This routine does the same as the above, however it is better for the RPC
* layer as it returns all data linearly in the vector 'out'. The output format is:
*    |ID|Length|Path|ID|Length|Path| ...
* where:
*  - ID = the type of the path, i.e.: DescriptorFile or DescriptorDirectory (1 byte);
*  - Length = The length of the path that follows (uint32_t - 4 bytes);
*  - Path = The actual path of the file/directory discovered (Length bytes).
* Finally, the 'size' is an convenience parameter to return the amount of entries
* discovered (optional).
*/
int ListFileEntriesLinear(char *basePath, std::vector<uint8_t> &out,
                          uint32_t *size=nullptr);

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
* Computes the ratio of values in 'data' that are actually repsentable under
* the encoding providing by 'StringToCodepoint'. This returns the ratio:
*                 ratio = failed / (succeeded + failed)
* This routine is computed while ratio is less than 'kMaximumDecodeRatio',
* it can be used to perform a simple detection for binary files or files that
* are potentially dangerous to open or need special handling as the default handlers
* won't work on them.
*/
double ComputeDecodeRatio(char *data, uint size);

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
* Get the size and content of a UTF-8 character at a given position.
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
* Computes the raw position inside string 's0' with size 'len' that correspondes
* to the encoded character located at 'u8p'. Optionally can pass a 'size' parameter
* to return the size in bytes of the encoded character.
*/
uint StringComputeRawPosition(char *s0, uint len, uint u8p, int *size);

/*
* Returns the address in 'path' where printing should start in order to display
* the last 'width' folders/files that are compounded in 'path'.
* Example: if path = C:/dev/folder1/folder2/file.txt and width = 2 than the return is
* at = 15 (f) because:
*                5   4     3       2       1
*                C:/dev/folder1/folder2/file.txt
*                0123456789abcde^
*/
uint StringCompressPath(char *path, uint size, uint width);

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
* Checks if a string contains an extension, mostly for GLX/GL stuff
*/
int ExtensionStringContains(const char *string, const char *extensions);

/*
* Bad 16 bits random numbers.
*/
uint Bad_RNG16();

/* Standard memcpy */
void Memcpy(void *dst, void *src, uint size);
/* Standard memset */
void Memset(void *dst, unsigned char v, uint size);

/*
* Utility for reading string line-by-line.
*/
template<typename Fn> inline
void ReadStringLineByLine(char *content, uint contentSize, Fn fn){
    uint iterator = 0;
    char *head = nullptr;

    if(!content || contentSize == 0)
        goto __end;

    head = content;
    while(iterator < contentSize){
        if(content[iterator] == '\n'){
            uint len = &content[iterator] - head;
            content[iterator] = 0;
            fn(head, len);

            content[iterator] = '\n';
            head = &content[iterator+1];
        }

        iterator += 1;
    }

__end:
    return;
}

/*
* Utilities for measuring the time a function takes to execute,
* this is mostly for debug, not a reliable measurement. Each line has '\n' replaced
* with '\0'.
*/
template<typename Fn>
inline double MeasureInterval(const Fn &fn){
    clock_t start = clock();
    fn();
    return double(clock() - start)/CLOCKS_PER_SEC;
}

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

template<typename T> inline void List_Init(List<T> *list){
    AssertA(list != nullptr, "list is not valid");
    list->size = 0;
    list->head = nullptr;
    list->tail = nullptr;
}

template<typename T> inline List<T> *List_Create(){
    List<T> *list = AllocatorGetN(List<T>, 1);
    AssertA(list != nullptr, "Failed to get list memory");
    List_Init<T>(list);
    return list;
}

template<typename T> inline T *List_GetTail(List<T> *list){
    return list->tail->item;
}

template<typename T> inline uint List_Size(List<T> *list){
    return list->size;
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

/*
* For lambdas use this.
*/
template<typename T, typename Fn> inline
void List_ForAllItems(List<T> *list, Fn fn){
    if(!list) return;
    ListNode<T> *ax = list->head;
    while(ax != nullptr){
        fn(ax->item);
        ax = ax->next;
    }
}

template<typename T, typename Fn> inline
void List_ForAllItemsInterruptable(List<T> *list, Fn fn){
    if(!list) return;
    ListNode<T> *ax = list->head;
    while(ax != nullptr){
        if(fn(ax->item) == 0){
            break;
        }
        ax = ax->next;
    }
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

template<typename T, typename Fn> inline
void List_Erase(List<T> *list, Fn finder){
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

template<typename Item, typename Fn> inline
void PriorityQueue_ForAllItems(PriorityQueue<Item> *pq, Fn fn){
    if(!pq) return;
    for(uint i = 0; i < pq->size; i++){
        if(fn(pq->entries[i].data) == 0)
            break;
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
* 'Circular' stack with fixed length.
*/
template<typename T>
struct CircularStack{
    T *items;
    uint capacity;
    uint size;
    int top;
};

template<typename T> inline void CircularStack_Init(CircularStack<T> *cstack, uint n){
    AssertA(cstack != nullptr, "Invalid pointer to circular stack");
    cstack->capacity = Max(n, 1);
    cstack->top = 0;
    cstack->size = 0;
    cstack->items = AllocatorGetN(T, cstack->capacity);
}

template<typename T> inline CircularStack<T> *CircularStack_Create(uint n){
    CircularStack<T> *cstack = (CircularStack<T> *)AllocatorGet(sizeof(CircularStack<T>));
    AssertA(cstack != nullptr, "Failed to get memory for ciruclar stack");
    CircularStack_Init(cstack, n);
    return cstack;
}

template<typename T> inline void CircularStack_Push(CircularStack<T> *cstack, T *item){
    if(cstack->capacity == 0) return;
    int pos = (cstack->top + 1) % (cstack->capacity);
    cstack->items[cstack->top] = *item;
    if(cstack->size < cstack->capacity) cstack->size += 1;
    cstack->top = pos;
}

template<typename T> inline uint CircularStack_Size(CircularStack<T> *cstack){
    return cstack->size;
}

template<typename T> inline T *CircularStack_At(CircularStack<T> *cstack, uint at){
    uint where = 0;
    if(cstack->capacity == 0 || at >= cstack->size) return nullptr;

    if(cstack->size == cstack->capacity){
        where = cstack->top > 0 ? cstack->top-1 : cstack->capacity-1;
        // it is actually very simple if the size is already equal to capacity
        // it means top wrapped and we simply need to count forward
        while(at > 0){
            where = where > 0 ? where-1 : cstack->capacity-1;
            at--;
        }
    }else{
        // if not than simply count from the 0 which means where = at
        where = (cstack->top-1) - at;
    }

    return &cstack->items[where];
}

template<typename T> inline void CircularStack_Clear(CircularStack<T> *cstack){
    cstack->top = 0;
    cstack->size = 0;
}

template<typename T> inline void CircularStack_Free(CircularStack<T> *cstack){
    cstack->top = 0;
    cstack->size = 0;
    cstack->capacity = 0;
    AllocatorFree(cstack->items);
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
