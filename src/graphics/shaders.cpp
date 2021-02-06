#include <shaders.h>
#include <utilities.h>
#include <string>
#include <map>
#include <sstream>
#include <glad/glad.h>
#include <string.h>

#define MODULE_NAME "Shader"

#define IS_SPACE(x) (((x) == ' ') || ((x) == '\t'))
#define IS_NEW_LINE(x) (((x) == '\r') || ((x) == '\n') || ((x) == '\0'))
#define IS_DIGIT(x) (static_cast<unsigned int>((x) - '0') < static_cast<unsigned int>(10))
#define CAN_SKIP(x) (IS_SPACE(x) && !IS_NEW_LINE(x))

/* Persist shaders for dynamic inclusion */
static std::map<std::string, std::string> ShaderStorage;

int Shader_CheckForCompileErrors(GLuint object, std::string type){
    int err = 0;
    GLint success;
    GLchar infoLog[1024];
    GLboolean exist;
    glGetBooleanv(GL_SHADER_COMPILER, &exist);
    if(type != "PROGRAM"){
        if(exist){
            glGetShaderiv(object, GL_COMPILE_STATUS, &success);
            if(!success){
                std::stringstream ss;
                std::string str;
                glGetShaderInfoLog(object, 1024, NULL, infoLog);
                ss << "| ERROR::SHADER: Compile-time error: Type: "
                    << type << "\n" << infoLog <<
                    "\n -- --------------------------------------------------- -- "
                    << std::endl;
                err = 1;
                str = ss.str();
                DEBUG_MSG("%s", str.c_str());
            }
        }else{
            DEBUG_MSG("It seems we do not have a SHADER COMPILER available\n");
            err = 1;
        }
    }else{
        glGetProgramiv(object, GL_LINK_STATUS, &success);
        if(exist){
            if(!success){
                std::stringstream ss;
                std::string str;
                glGetProgramInfoLog(object, 1024, NULL, infoLog);
                ss << "| ERROR::Shader: Link-time error: Type: "
                    << type << "\n" << infoLog <<
                    "\n -- --------------------------------------------------- -- "
                    << std::endl;
                err = 1;
                str = ss.str();
                DEBUG_MSG("%s", str.c_str());
            }
        }
    }
    
    return err;
}

void Shader_StoragePublic(const std::string &filename, const std::string &content){
    ShaderStorage[filename] = content;
}

void Shader_StoragePublic(const char *path){
    char *p = nullptr;
    char *content = nullptr;
    uint size = 0;
    
    int addr = GetRightmostSplitter(path, strlen(path));
    p = (char *)&path[addr+1];
    
    content = GetFileContents(path, &size);
    if(content != nullptr){
        Shader_StoragePublic(p, std::string(content));
        free(content);
    }else{
        DEBUG_MSG("Failed to read file %s\n", path);
    }
}

int Shader_StorageQueryContent(const std::string &shadername, std::string &content){
    int rv = -1;
    if(ShaderStorage.find(shadername) != ShaderStorage.end()){
        content = ShaderStorage[shadername];
        rv = 0;
    }
    
    return rv;
}

int Shader_TranslateShaderContent(const std::string &content, std::string &data){
    std::istringstream iss(content);
    int fullParse = 1;
    std::string work;
    for(std::string linebuf; std::getline(iss, linebuf); ){
        RemoveUnwantedLineTerminators(linebuf);
        if(linebuf.empty()) continue;
        const char *token = linebuf.c_str();
        
        if(linebuf.find("#include") == 0){
            std::string subs = linebuf.substr(8);
            const char *token = subs.c_str();
            while(CAN_SKIP(token[0])) token++;
            std::string path(token);
            std::string included;
            if(Shader_StorageQueryContent(path, included) != 0){
                DEBUG_MSG("Could not find path %s\n", path.c_str());
                fullParse = 0;
                work.clear();
                break;
            }
            
            work += included;
        }else{
            work += linebuf;
        }
        
        work += "\n";
    }
    
    if(fullParse) data = work;
    return fullParse;
}

int Shader_CompileFile(const char *path, int type, char *oContent){
    int rv = -1;
    uint id = 0;
    uint filesize = 0;
    std::string translated;
    uint gltype = GL_VERTEX_SHADER;
    const char *v = "VERTEX";
    const char *f = "FRAGMENT";
    char *str = (char *)v;
    char *p = nullptr;
    
    if(type != SHADER_TYPE_VERTEX && 
       type != SHADER_TYPE_FRAGMENT || path == nullptr) return rv;
    
    char *content = GetFileContents(path, &filesize);
    if(content == nullptr) return rv;
    
    if(Shader_TranslateShaderContent(content, translated) != 1) goto end;
    
    if(type == SHADER_TYPE_FRAGMENT){
        str = (char *)f;
        gltype = GL_FRAGMENT_SHADER;
    }
    
    id = glCreateShader(gltype);
    if(id == 0){
        DEBUG_MSG("Failed to create shader");
        goto end;
    }
    
    p = (char *)translated.c_str();
    glShaderSource(id, 1, (char **)&p, NULL);
    glCompileShader(id);
    
    if(Shader_CheckForCompileErrors(id, str)) goto end;
    
    if(oContent){
        oContent = content;
        content = nullptr;
    }
    
    rv = (int)id;
    
    end:
    if(content) AllocatorFree(content);
    return rv;
}

int Shader_Create(Shader &shader, uint vertex, uint fragment){
    int rv = -1;
    
    shader.id = glCreateProgram();
    glAttachShader(shader.id, vertex);
    glAttachShader(shader.id, fragment);
    glLinkProgram(shader.id);
    
    if(Shader_CheckForCompileErrors(shader.id, "PROGRAM") == 0){
        rv = 0;
    }
    
    return rv;
}

static std::map<std::string, int> loggedMap;
void Shader_UniformInteger(Shader &shader, const char *name, int value){
    int id = glGetUniformLocation(shader.id, name);
    if(id >= 0){
        glUniform1i(id, value);
    }else{
        if(loggedMap.find(name) == loggedMap.end()){
            DEBUG_MSG("Failed to load uniform %s\n", name);
            loggedMap[name] = 1;
        }
    }
}

void Shader_UniformMatrix4(Shader &shader, const char *name, Matrix4x4 *matrix){
    Float *rawMatrix = (Float *)matrix->m;
    int id = glGetUniformLocation(shader.id, name);
    if(id >= 0){
        glUniformMatrix4fv(id, 1, GL_FALSE, rawMatrix);
    }else{
        if(loggedMap.find(name) == loggedMap.end()){
            DEBUG_MSG("Failed to load uniform %s\n", name);
            loggedMap[name] = 1;
        }
    }
}