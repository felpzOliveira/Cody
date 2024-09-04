#include <image_renderer.h>
#include <graphics.h>

static GLuint quadVAO;
static GLuint quadVBO;
static GLuint quadEBO;


const char *vertexImgContent = "#version 450 core\n"
"layout(location = 0) in vec3 inPosition;\n"
"layout(location = 1) in vec2 inTexCoord;\n"
"out vec2 TexCoords;\n"
"void main(){\n"
"    TexCoords = inTexCoord;\n"
"    gl_Position = vec4(inPosition, 1.0);\n"
"}";

const char *borderFragmentImg = "#version 450 core\n"
"uniform vec4 backgroundColor;\n"
"uniform float transitionFactor;\n"
"uniform float viewActive;\n"
"in vec2 TexCoords;\n"
"out vec4 fragColor;\n"
"void main(){\n"
"    vec2 uv = TexCoords;\n"
"    float u0 = 0.0025;\n"
"    float u1 = 0.9975;\n"
"    float v0 = 0.0025;\n"
"    float v1 = 0.9975;\n"
"    vec4 baseColor = backgroundColor;\n"
"    if(uv.x > u1){\n"
"        vec4 redColor = vec4(1.0, 1.0, 0.0, 1.0);\n"
"        baseColor = mix(redColor, backgroundColor, transitionFactor);\n"
"    }else if(uv.x < u0 && viewActive > 0){\n"
"        vec4 redColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
"        baseColor = mix(redColor, backgroundColor, 0.5);\n"
"    }else{\n"
"        discard;\n"
"    }\n"
"    fragColor = baseColor;\n"
"}";


const char *fragmentImgContent = "#version 450 core\n"
"uniform sampler2D image0;\n"
"uniform sampler2D imageMinus1;\n"
"uniform sampler2D imagePlus1;\n"
"uniform float width;\n"
"uniform float height;\n"
"uniform vec2 zoomCenter;\n"
"uniform float zoomLevel;\n"
"uniform vec3 actives;\n"
"uniform vec4 backgroundColor;\n"
"in vec2 TexCoords;\n"
"out vec4 fragColor;\n"
"vec2 zoom_at(vec2 uv, vec2 zoomCenter, float zoomLevel, out float samplerId){\n"
"    samplerId = 1;\n"
"    float d = 1.0 / zoomLevel;\n"
"    float x_0 = zoomCenter.x - d * 0.5;\n"
"    float y_0 = zoomCenter.y - d * 0.5;\n"
"    uv = vec2(x_0, y_0) + uv * d;\n"
"    if((uv.y < 0 && uv.y > -0.0075) ||\n"
"       (uv.y > 1 && uv.y < 1.0075)){\n"
"        samplerId = -2;\n"
"    }else if(uv.y < 0){\n"
"        uv.y += 1.0;\n"
"        samplerId = 2;\n"
"        if(actives.z < 1)\n"
"            samplerId = -2;\n"
"    }else if(uv.y > 1){\n"
"        uv.y -= 1.0;\n"
"        samplerId = -1;\n"
"        if(actives.x < 1)\n"
"            samplerId = -2;\n"
"    }\n"
"    return uv;\n"
"}\n"
"vec4 eval_texture(vec2 uv, vec2 texSize, sampler2D sampler){\n"
"    if(zoomLevel < 1.02){\n"
"        const int d_radius = 1;\n"
"        vec4 result = vec4(0);\n"
"        vec2 texelSize = 1.0 / texSize;\n"
#if 0
"        result += texture2D(sampler, uv + vec2(1,1)   * texelSize);\n"
"        result += texture2D(sampler, uv + vec2(1,-1)  * texelSize);\n"
"        result += texture2D(sampler, uv + vec2(-1,1)  * texelSize);\n"
"        result += texture2D(sampler, uv + vec2(-1,-1) * texelSize);\n"
"        result += texture2D(sampler, uv + vec2(2,1)   * texelSize);\n"
"        result += texture2D(sampler, uv + vec2(2,-1)  * texelSize);\n"
"        result += texture2D(sampler, uv + vec2(-2,1)  * texelSize);\n"
"        result += texture2D(sampler, uv + vec2(-2,-1) * texelSize);\n"
"        result += texture2D(sampler, uv + vec2(1,2)   * texelSize);\n"
"        result += texture2D(sampler, uv + vec2(1,-2)  * texelSize);\n"
"        result += texture2D(sampler, uv + vec2(-1,2)  * texelSize);\n"
"        result += texture2D(sampler, uv + vec2(-1,-2) * texelSize);\n"
"        result += texture2D(sampler, uv + vec2(2,2)   * texelSize);\n"
"        result += texture2D(sampler, uv + vec2(2,-2)  * texelSize);\n"
"        result += texture2D(sampler, uv + vec2(-2,2)  * texelSize);\n"
"        result += texture2D(sampler, uv + vec2(-2,-2) * texelSize);\n"
"        return result * 0.0625;\n"
#else
"        result += texture2D(sampler, uv + vec2(1,1)   * texelSize);\n"
"        result += texture2D(sampler, uv + vec2(1,-1)  * texelSize);\n"
"        result += texture2D(sampler, uv + vec2(-1,1)  * texelSize);\n"
"        result += texture2D(sampler, uv + vec2(-1,-1) * texelSize);\n"
"        return result * 0.25;\n"
#endif
"    }else{\n"
"        return texture2D(sampler, uv);\n"
"    }\n"
"}\n"
"vec4 eval_texture_wrap(vec2 uv, vec2 texSize, float samplerId){\n"
"    if(samplerId < -1)\n"
"        return backgroundColor;\n"
"    else if(samplerId < 0)\n"
"        return eval_texture(uv, texSize, imageMinus1);\n"
"    else if(samplerId > 1.5)\n"
"        return eval_texture(uv, texSize, imagePlus1);\n"
"    else\n"
"        return eval_texture(uv, texSize, image0);\n"
"}\n"
"void main(){\n"
"    vec4 imageColor;"
"    vec2 uv = TexCoords;\n"
"    float blendFactor = 0.0;\n"
"    float border_width = 0.005 * 800.0 / width;\n"
"    float samplerId = 0.0;\n"
"    uv = zoom_at(uv, zoomCenter, zoomLevel, samplerId);\n"
"    vec2 imageSize = vec2(textureSize(image0, 0));\n"
"    float imageAspect = imageSize.x / imageSize.y;\n"
"    float targetHeight = height;\n"
"    float targetWidth = targetHeight * imageAspect;\n"
"    if(targetWidth > width){\n"
"        targetWidth = width;\n"
"        targetHeight = targetWidth / imageAspect;\n"
"        if(targetHeight > height){\n"
"            fragColor = vec4(1.0, 0.0, 1.0, 1.0);\n" // impossible to render??
"        }else{\n"
"            float dv = 1.0 - (targetHeight / height);\n"
"            float v_0 = dv * 0.5;\n"
"            float v_1 = 1.0 - v_0;\n"
"            if(uv.y > v_1 || uv.y < v_0){\n"
"                imageColor = backgroundColor;\n"
"            }else{\n"
"                uv.y = (uv.y - v_0) / (v_1 - v_0);\n"
"                fragColor = eval_texture_wrap(uv, imageSize, samplerId);\n"
"            }\n"
"        }\n"
"    }else{\n"
"        float du = 1.0 - (targetWidth / width);\n"
"        float u_0 = du * 0.5;\n"
"        float u_1 = 1.0 - u_0;\n"
"        if(uv.x < u_0 || uv.x > u_1){\n"
"            imageColor = backgroundColor;\n"
"        }else{\n"
"            uv.x = (uv.x - u_0) / (u_1 - u_0);\n"
"            imageColor = eval_texture_wrap(uv, imageSize, samplerId);\n"
"        }\n"
"    }\n"
"    fragColor = imageColor;\n"
"}";



void InitializeImageRendererQuad(){
    int rv = -1;
    // Vertex data for a quad (two triangles)
    GLfloat vertices[] = {
        -1.0f, -1.0f, 0.0f,  // Bottom-left corner
        1.0f, -1.0f, 0.0f,   // Bottom-right corner
        1.0f, 1.0f, 0.0f,    // Top-right corner
        -1.0f, 1.0f, 0.0f    // Top-left corner
    };

    // Texture coordinates
    GLfloat texCoords[] = {
        0.0f, 0.0f,  // Bottom-left corner
        1.0f, 0.0f,  // Bottom-right corner
        1.0f, 1.0f,  // Top-right corner
        0.0f, 1.0f   // Top-left corner
    };

    // Index data (for rendering as triangles)
    GLuint indices[] = {
        0, 1, 2,  // First triangle
        2, 3, 0   // Second triangle
    };

    OpenGLCHK(glGenVertexArrays(1, &quadVAO));
    OpenGLCHK(glGenBuffers(1, &quadVBO));
    OpenGLCHK(glGenBuffers(1, &quadEBO));
    OpenGLCHK(glBindVertexArray(quadVAO));

    OpenGLCHK(glBindBuffer(GL_ARRAY_BUFFER, quadVBO));
    OpenGLCHK(glBufferData(GL_ARRAY_BUFFER, sizeof(vertices) + sizeof(texCoords),
                           nullptr, GL_STATIC_DRAW));
    OpenGLCHK(glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices));
    OpenGLCHK(glBufferSubData(GL_ARRAY_BUFFER, sizeof(vertices),
                              sizeof(texCoords), texCoords));

    OpenGLCHK(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat),
                                    (GLvoid*)0));
    OpenGLCHK(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat),
                                    (GLvoid*)sizeof(vertices)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    OpenGLCHK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadEBO));
    OpenGLCHK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices),
                               indices, GL_STATIC_DRAW));
    rv = 0;
__ret:
    if(rv != 0)
        exit(0);
}

static
void ImageRenderer_AllocateTexture(GLuint texId, int width, int height){
    OpenGLCHK(glTextureParameteri(texId, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    OpenGLCHK(glTextureParameteri(texId, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    OpenGLCHK(glTextureParameteri(texId, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    OpenGLCHK(glTextureParameteri(texId, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    OpenGLCHK(glTextureStorage2D(texId, 1, GL_RGBA8, width, height));
}

void ImageRendererCreate(ImageRenderer &renderer, PdfRenderPages pages){
    int width = pages.centerPage.width;
    int height = pages.centerPage.height;
    unsigned char *image = pages.centerPage.pixels;
    renderer.active = vec3f(0, 1, 0);

    OpenGLCHK(glCreateTextures(GL_TEXTURE_2D, 3, renderer.texture));
    ImageRenderer_AllocateTexture(renderer.texture[0], width, height);
    OpenGLCHK(glTextureSubImage2D(renderer.texture[0], 0, 0, 0,
                        width, height, GL_RGBA, GL_UNSIGNED_BYTE, image));

    ImageRenderer_AllocateTexture(renderer.texture[1], width, height);
    if(pages.previousPage.pixels){
        image = pages.previousPage.pixels;
        OpenGLCHK(glTextureSubImage2D(renderer.texture[1], 0, 0, 0,
                        width, height, GL_RGBA, GL_UNSIGNED_BYTE, image));
        renderer.active.x = 1;
    }

    ImageRenderer_AllocateTexture(renderer.texture[2], width, height);
    if(pages.nextPage.pixels){
        image = pages.nextPage.pixels;
        OpenGLCHK(glTextureSubImage2D(renderer.texture[2], 0, 0, 0,
                        width, height, GL_RGBA, GL_UNSIGNED_BYTE, image));
        renderer.active.z = 1;
    }

    renderer.Inited();
}

void ImageRendererCleanup(ImageRenderer &renderer){
    if(renderer.IsInited()){
        glDeleteTextures(3, renderer.texture);
        renderer.texture[0] = 0;
        renderer.texture[1] = 0;
        renderer.texture[2] = 0;
        renderer.inited = false;
    }
}

void ImageRendererUpdate(ImageRenderer &renderer, PdfGraphicsPage page){
    if(!renderer.IsInited())
        return;

    int width = page.width;
    int height = page.height;
    unsigned char *image = page.pixels;
    OpenGLCHK(glTextureSubImage2D(renderer.texture[0], 0, 0, 0,
                        width, height, GL_RGBA, GL_UNSIGNED_BYTE, image));
    renderer.active = vec3f(0, 1, 0);
}

void ImageRendererUpdate(ImageRenderer &renderer, PdfRenderPages pages){
    if(!renderer.IsInited())
        return;

    int width = pages.centerPage.width;
    int height = pages.centerPage.height;
    unsigned char *image = pages.centerPage.pixels;
    OpenGLCHK(glTextureSubImage2D(renderer.texture[0], 0, 0, 0,
                        width, height, GL_RGBA, GL_UNSIGNED_BYTE, image));

    renderer.active = vec3f(0, 1, 0);
    if(pages.previousPage.pixels){
        image = pages.previousPage.pixels;
        OpenGLCHK(glTextureSubImage2D(renderer.texture[1], 0, 0, 0,
                        width, height, GL_RGBA, GL_UNSIGNED_BYTE, image));
        renderer.active.x = 1;
    }

    if(pages.nextPage.pixels){
        image = pages.nextPage.pixels;
        OpenGLCHK(glTextureSubImage2D(renderer.texture[2], 0, 0, 0,
                        width, height, GL_RGBA, GL_UNSIGNED_BYTE, image));
        renderer.active.z = 1;
    }
}

void ImageRendererRender(ImageRenderer &renderer, Shader &shaderImg, Shader &shaderBorder,
                         vec4f backgroundColor, Float factor,  Geometry *geometry,
                         vec2f zoomCenter, Float zoomLevel, int active)
{
    if(!renderer.IsInited())
        return;

    glDisable(GL_BLEND);

    glUseProgram(shaderImg.id);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadEBO);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glActiveTexture(GL_TEXTURE0);

    OpenGLCHK(glBindTextureUnit(0, renderer.texture[0]));
    OpenGLCHK(glBindTextureUnit(1, renderer.texture[1]));
    OpenGLCHK(glBindTextureUnit(2, renderer.texture[2]));
    Shader_UniformInteger(shaderImg, "image0", 0);
    Shader_UniformInteger(shaderImg, "imageMinus1", 1);
    Shader_UniformInteger(shaderImg, "imagePlus1", 2);
    Shader_UniformVec3(shaderImg, "actives", renderer.active);

    Shader_UniformFloat(shaderImg, "width", geometry->Width());
    Shader_UniformFloat(shaderImg, "height", geometry->Height());
    Shader_UniformVec2(shaderImg, "zoomCenter", zoomCenter);
    Shader_UniformFloat(shaderImg, "zoomLevel", zoomLevel);
    Shader_UniformVec4(shaderImg, "backgroundColor", backgroundColor);

    OpenGLCHK(glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, 1));

    glUseProgram(shaderBorder.id);
    Shader_UniformVec4(shaderBorder, "backgroundColor", backgroundColor);
    Shader_UniformFloat(shaderBorder, "transitionFactor", factor);
    Shader_UniformFloat(shaderBorder, "viewActive", active);
    OpenGLCHK(glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, 1));

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glUseProgram(0);
}
