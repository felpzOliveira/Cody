#include <shaders.h>
#include <utilities.h>
#include <string>
#include <map>
#include <sstream>
#include <glad/glad.h>
#include <string.h>
#include <app.h>
#include <thread>
#include <iostream>

#define MODULE_NAME "Shader"

#define IS_SPACE(x) (((x) == ' ') || ((x) == '\t'))
#define IS_NEW_LINE(x) (((x) == '\r') || ((x) == '\n') || ((x) == '\0'))
#define IS_DIGIT(x) (static_cast<unsigned int>((x) - '0') < static_cast<unsigned int>(10))
#define CAN_SKIP(x) (IS_SPACE(x) && !IS_NEW_LINE(x))

/* Persist shaders for dynamic inclusion */
static std::map<std::string, std::string> ShaderStorage;
static std::map<std::string, int> loggedMap;
static std::map<int, std::thread::id> progMap;

std::string OpenGLValidateErrorStr(int *err);

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

int Shader_CompileSource(const std::string &content, int type){
    int rv = -1;
    uint id = 0;
    std::string translated;
    uint gltype = GL_VERTEX_SHADER;
    const char *v = "VERTEX";
    const char *f = "FRAGMENT";
    char *str = (char *)v;
    char *p = nullptr;

    if((type != SHADER_TYPE_VERTEX &&
        type != SHADER_TYPE_FRAGMENT) || content.size() < 1) return rv;

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
    OpenGLCHK(glShaderSource(id, 1, (char **)&p, NULL));
    OpenGLCHK(glCompileShader(id));

    if(Shader_CheckForCompileErrors(id, str)) goto end;

    rv = (int)id;

end:
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
        progMap[shader.id] = std::this_thread::get_id();
    }

    return rv;
}

static void Shader_LogUniformError(Shader &shader, const char *name){
    std::string msg;
    char pm[256];
    bool isProg = glIsProgram(shader.id);
    std::string err = OpenGLValidateErrorStr(nullptr);

    if(progMap.find(shader.id) == progMap.end()){
        snprintf(pm, 256, "Program %d was never created", shader.id);
    }else{
        std::thread::id id = std::this_thread::get_id();
        if(id == progMap[shader.id]){
            snprintf(pm, 256, "Program %d seems valid", shader.id);
        }else{
            snprintf(pm, 256, "Program %d was created by different thread", shader.id);
        }
    }

    msg = std::string(pm);

    loggedMap[name] = 1;
    DEBUG_MSG("Failed to load uniform %s : %s ( Is prog: %d )\n > Msg: %s\n",
              name, err.c_str(), (int)isProg, msg.c_str());
}

void Shader_UniformInteger(Shader &shader, const char *name, int value){
    OpenGLClearErrors();
    int id = glGetUniformLocation(shader.id, name);
    if(id >= 0){
        glUniform1i(id, value);
    }else{
        if(loggedMap.find(name) == loggedMap.end()){
            Shader_LogUniformError(shader, name);
        }
    }
}

void Shader_UniformFloat(Shader &shader, const char *name, Float value){
    OpenGLClearErrors();
    int id = glGetUniformLocation(shader.id, name);
    if(id >= 0){
        glUniform1f(id, value);
    }else{
        if(loggedMap.find(name) == loggedMap.end()){
            Shader_LogUniformError(shader, name);
        }
    }
}

void Shader_UniformVec4(Shader &shader, const char *name, vec4f value){
    OpenGLClearErrors();
    int id = glGetUniformLocation(shader.id, name);
    if(id >= 0){
        glUniform4f(id, value.x, value.y, value.z, value.w);
    }else{
        if(loggedMap.find(name) == loggedMap.end()){
            Shader_LogUniformError(shader, name);
        }
    }
}

void Shader_UniformVec2(Shader &shader, const char *name, vec2f value){
    OpenGLClearErrors();
    int id = glGetUniformLocation(shader.id, name);
    if(id >= 0){
        glUniform2f(id, value.x, value.y);
    }else{
        if(loggedMap.find(name) == loggedMap.end()){
            Shader_LogUniformError(shader, name);
        }
    }
}

void Shader_UniformMatrix4(Shader &shader, const char *name, Matrix4x4 *matrix){
    OpenGLClearErrors();
    Float *rawMatrix = (Float *)matrix->m;
    int id = glGetUniformLocation(shader.id, name);
    if(id >= 0){
        glUniformMatrix4fv(id, 1, GL_FALSE, rawMatrix);
    }else{
        if(loggedMap.find(name) == loggedMap.end()){
            Shader_LogUniformError(shader, name);
        }
    }
}
