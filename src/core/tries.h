#if !defined(TRIES_H)
#define TRIES_H
#include <types.h>
#include <unordered_map>
#include <functional>
#include <string>
#include <encoding.h>

#define TRIE_NODE_INITIALIZER { .u8Size = 0, .codepoint = 0, .word_count = 0, }

typedef struct Trie{
    uint u8Size;
    int codepoint;
    uint word_count;
    CharU8 key;
    std::unordered_map<int, Trie *> children;
    EncoderDecoder encoder;
}Trie;

/*
* Inserts a word given in 'value' into the given trie 'root'.
*/
int Trie_Insert(Trie *root, char *value, uint valuelen);

/*
* Perform a search based on the infix given in 'value' applying 'fn' on every
* word that matches.
*/
void Trie_Search(Trie *root, char *value, uint valuelen,
                 std::function<void(char *, uint, Trie *)> fn);

/*
* Removes a word given in 'value' from a given trie.
*/
int Trie_Remove(Trie *root, char *value, uint valuelen);

#endif // TRIES_H
