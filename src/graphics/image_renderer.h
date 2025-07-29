/* date = June 11th 2024 22:20 */
#pragma once
#include <geometry.h>
#include <glad/glad.h>
#include <shaders.h>
#include <timer.h>
#include <pdfview.h>

class ImageRenderer{
public:
    GLuint texture[3];
    vec3f active;
    bool inited;
    bool transRunning;
    double startInterval;
    double endInterval;

    ImageRenderer(){
        texture[0] = 0;
        texture[1] = 0;
        texture[2] = 0;
        inited = false;
        transRunning = false;
    }
    bool IsInited(){ return inited; }
    void Inited(){ inited = true; }
    void BeginTransition(double _duration){
        startInterval = GetElapsedTime();
        endInterval = startInterval + _duration;
        transRunning = true;
    }

    bool IsTransitioninig(){ return transRunning; }
    double Transition(){
        if(!transRunning)
            return 1.0f;

        double curr = GetElapsedTime();
        double value = (curr - startInterval) / (endInterval - startInterval);
        if(value > 1){
            value = 1;
            transRunning = false;
        }

        return value;
    }
};

void ImageRendererCreate(ImageRenderer &renderer, PdfRenderPages pages);

void ImageRendererCleanup(ImageRenderer &renderer);

void ImageRendererUpdate(ImageRenderer &renderer, PdfRenderPages pages);

void ImageRendererUpdate(ImageRenderer &renderer, PdfGraphicsPage page);

void ImageRendererRender(ImageRenderer &renderer, Shader &shaderImg, Shader &shaderBorder,
                         vec4f backgroundColor, Float tFactor, Geometry *geometry,
                         vec2f zoomCenter, Float zoomLevel, int active);

void InitializeImageRendererQuad();
