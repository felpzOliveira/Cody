#include <graphics.h>

void Graphics_InitializeOpenGL(Graphics_Opts *opts);

void Graphics_Initialize(Graphics_Opts *opts){
    Graphics_InitializeOpenGL(opts);
}