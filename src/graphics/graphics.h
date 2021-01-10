/* date = January 10th 2021 10:12 am */

#ifndef GRAPHICS_H
#define GRAPHICS_H

//TODO: Set everything related to opening/options of a context
typedef struct{
    int is_opengl;
}Graphics_Opts;

void Graphics_Initialize(Graphics_Opts *opts);

#endif //GRAPHICS_H
