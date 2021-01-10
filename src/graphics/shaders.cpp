#include <shaders.h>
#include <utilities.h>
#include <string>
#include <map>

#define MODULE_NAME "Shader"

#define IS_SPACE(x) (((x) == ' ') || ((x) == '\t'))
#define IS_NEW_LINE(x) (((x) == '\r') || ((x) == '\n') || ((x) == '\0'))
#define IS_DIGIT(x) (static_cast<unsigned int>((x) - '0') < static_cast<unsigned int>(10))
#define CAN_SKIP(x) (IS_SPACE(x) && !IS_NEW_LINE(x))

/* Persist shaders for dynamic inclusion */
static std::map<std::string, std::string> ShaderStorage;

void Shader_StoragePublic(const std::string &filename, const std::string &content){
    ShaderStorage[filename] = content;
}

void Shader_StoragePublic(const char *path){
    char *p = nullptr;
    char *content = nullptr;
    uint size = 0;
    
    int addr = GetRightmostSplitter(path, strlen(path));
    p = &path[addr+1];
    
    content = GetFileContents(path, &size);
    if(content != nullptr){
        Shader_StoragePublic(p, std::string(content));
        free(content);
    }else{
        DEBUG_MSG("Failed to read file %s\n", path);
    }
}

