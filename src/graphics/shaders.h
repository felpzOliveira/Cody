/* date = January 10th 2021 6:20 pm */

#ifndef SHADERS_H
#define SHADERS_H

#include <types.h>

/*
* Basic OpenGL shaders support
*/
typedef struct{
    uint id;
    char *vertexShaderFile;
    char *fragmentShaderFile;
}Shader;

void Shader_StoragePublic(const std::string &filename, const std::string &content);
void Shader_StoragePublic(const std::string &filename);

#endif //SHADERS_H
