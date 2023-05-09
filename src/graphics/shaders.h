/* date = January 10th 2021 6:20 pm */

#ifndef SHADERS_H
#define SHADERS_H

#include <types.h>
#include <string>
#include <transform.h>

#define SHADER_TYPE_VERTEX   0
#define SHADER_TYPE_FRAGMENT 1

/*
* Basic shaders support, this is not critical code, we are never going to be
* reloading shaders in a timed manner so we are going to abuse std::string.
*/
typedef struct{
    uint id;
    char *vertexShaderFile;
    char *fragmentShaderFile;
}Shader;

#define SHADER_INITIALIZER {.id = 0, .vertexShaderFile = nullptr, .fragmentShaderFile = nullptr}


void Shader_UniformMatrix4(Shader &shader, const char *name, Matrix4x4 *matrix);
void Shader_UniformInteger(Shader &shader, const char *name, int value);
void Shader_UniformFloat(Shader &shader, const char *name, Float value);
void Shader_UniformVec2(Shader &shader, const char *name, vec2f value);
void Shader_UniformVec4(Shader &shader, const char *name, vec4f value);
int Shader_Create(Shader &shader, uint vertex, uint fragment);

int Shader_CompileSource(const std::string &content, int type);

void Shader_StoragePublic(const std::string &filename, const std::string &content);
void Shader_StoragePublic(const std::string &filename);

#endif //SHADERS_H
