#if !defined(TRIES_H)
#define TRIES_H
#include <types.h>
#include <unordered_map>
#include <functional>
#include <string>

#define TRIE_NODE_INITIALIZER { .u8Size = 0, .codepoint = 0, .word_count = 0, }

typedef struct Trie{
    uint u8Size;
    int codepoint;
    uint word_count;
    CharU8 key;
    std::unordered_map<int, Trie *> children;
}Trie;

int Trie_Insert(Trie *root, char *value, uint valuelen);

void Trie_Transverse(Trie *root, std::function<void(char *, uint, Trie *)> fn);

void Trie_Search(Trie *root, char *value, uint valuelen,
                 std::function<void(char *, uint, Trie *)> fn);

int Trie_Remove(Trie *root, char *value, uint valuelen);

#endif // TRIES_H
