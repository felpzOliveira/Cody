#include <tries.h>
#include <utilities.h>

#define MAX_WORD_SIZE 128

int Trie_Insert(Trie *root, char *value, uint valuelen){
    Trie *current = nullptr;
    char *chr = value;
    int chrlen = 0;
    uint consumed = 0;
    if(!value || valuelen < 2 || !root) return -1;

    int cp = StringToCodepoint(chr, valuelen, &chrlen);
    int done = 0;
    if(cp == -1) return -1;

    current = root;
    while(done == 0){
        if(current->children.find(cp) == current->children.end()){
            Trie *newNode = new Trie; // Dangerous C++ allocation?
            Memcpy(newNode->key.x, &chr[consumed], chrlen);
            newNode->u8Size = chrlen;
            newNode->codepoint = cp;
            newNode->word_count = 0;
            current->children[cp] = newNode;

            current = newNode;
        }else{
            current = current->children[cp];
        }

        consumed += chrlen;
        if(valuelen - consumed == 0){
            current->word_count++;
            done = 1;
        }

        cp = StringToCodepoint(&chr[consumed], valuelen - consumed, &chrlen);
    }

    return 0;
}

int Trie_Remove(Trie *root, char *value, uint valuelen){
    Trie *current = nullptr;
    char *chr = value;
    int chrlen = 0;
    uint consumed = 0;
    Trie *stack[MAX_WORD_SIZE];
    uint stackAt = 0;
    if(!value || valuelen < 2 || !root || valuelen > MAX_WORD_SIZE) return -1;

    int cp = StringToCodepoint(chr, valuelen, &chrlen);
    if(cp == -1) return -1;

    current = root;
    stack[stackAt++] = nullptr;
    while(1){
        if(current->children.find(cp) == current->children.end()){
            return -1; // word does not exist
        }else{
            stack[stackAt++] = current;
            current = current->children[cp];
        }

        consumed += chrlen;
        if(valuelen - consumed == 0){
            // found node
            if(current->word_count == 0) return -1;

            current->word_count -= 1;
            if(current->word_count > 0) return 0;
            stackAt -= 1;

            while(current != nullptr && stackAt > 0 && current != root){
                if(current->children.size() == 0 && current->word_count == 0){
                    Trie *parent = stack[stackAt];
                    parent->children.erase(current->codepoint);
                    delete current;
                }else{ // there are children
                    break;
                }

                current = stack[stackAt--];
            }

            return 0;
        }

        cp = StringToCodepoint(&chr[consumed], valuelen - consumed, &chrlen);
    }
}

void _Trie_Transverse(Trie *current, std::function<void(char *, uint, Trie *)> fn,
                      char *buf, uint *at)
{
    if(current == nullptr) return;
    uint f = *at;
    Memcpy(&buf[f], current->key.x, current->u8Size);
    (*at) += current->u8Size;

    if(current->word_count > 0){
        char p = buf[(*at)];
        buf[(*at)] = 0;
        fn(buf, *at, current);
        buf[(*at)] = p;
    }

    std::unordered_map<int, Trie *>::iterator it;
    for(it = current->children.begin(); it != current->children.end(); it++){
        _Trie_Transverse(it->second, fn, buf, at);
    }

    buf[f] = 0;
    *at = f;
}

void Trie_Search(Trie *root, char *value, uint valuelen,
                 std::function<void(char *, uint, Trie *)> fn)
{
    char buf[MAX_WORD_SIZE];
    uint at = 0;
    Trie *current = nullptr;
    char *chr = value;
    int chrlen = 0;
    uint consumed = 0;
    if(!root || !value || valuelen == 0 || valuelen > MAX_WORD_SIZE) return;

    int cp = StringToCodepoint(chr, valuelen, &chrlen);
    int done = 0;
    if(cp == -1) return;

    current = root;

    while(done == 0){
        if(current->children.find(cp) == current->children.end()){
            return;
        }else{
            current = current->children[cp];
            if(consumed + chrlen < valuelen){
                Memcpy(&buf[at], current->key.x, current->u8Size);
                at += current->u8Size;
            }
        }

        consumed += chrlen;
        if(valuelen - consumed == 0){
            _Trie_Transverse(current, fn, buf, &at);
            return;
        }

        cp = StringToCodepoint(&chr[consumed], valuelen - consumed, &chrlen);
    }
}


void Trie_Transverse(Trie *root, std::function<void(char *, uint, Trie *)> fn){
    char value[256];
    uint at = 0;
    if(!root) return;

    _Trie_Transverse(root, fn, value, &at);
}
