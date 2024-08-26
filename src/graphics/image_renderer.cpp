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
"in vec2 TexCoords;\n"
"out vec4 fragColor;\n"
"void main(){\n"
"    vec2 uv = TexCoords;\n"
"    float u0 = 0.0025;\n"
"    float u1 = 0.9975;\n"
"    float v0 = 0.0025;\n"
"    float v1 = 0.9975;\n"
"    vec4 baseColor = backgroundColor;\n"
"    if(uv.x < u0 || uv.x > u1 || uv.y < v0 || uv.y > v1){\n"
"        vec4 redColor = vec4(1.0, 1.0, 0.0, 1.0);\n"
"        baseColor = mix(redColor, backgroundColor, transitionFactor);\n"
"    }else\n"
"        discard;\n"
"    fragColor = baseColor;\n"
"}";


const char *fragmentImgContent = "#version 450 core\n"
"uniform sampler2D image0;\n"
"uniform float width;\n"
"uniform float height;\n"
"uniform vec2 zoomCenter;\n"
"uniform float zoomLevel;\n"
"uniform vec4 backgroundColor;\n"
"in vec2 TexCoords;\n"
"out vec4 fragColor;\n"
"vec2 zoom_at(vec2 uv, vec2 zoomCenter, float zoomLevel){\n"
"    float d = 1.0 / zoomLevel;\n"
"    float x_0 = zoomCenter.x - d * 0.5;\n"
"    float y_0 = zoomCenter.y - d * 0.5;\n"
"    uv = vec2(x_0, y_0) + uv * d;\n"
"    return uv;\n"
"}\n"
"void main(){\n"
"    vec4 imageColor;"
"    vec4 outerColor = vec4(0.6, 0.6, 0.6, 1.0);\n"
"    vec2 uv = TexCoords;\n"
"    float blendFactor = 0.0;\n"
"    float border_width = 0.005 * 800.0 / width;\n"
"    uv = zoom_at(uv, zoomCenter, zoomLevel);\n"
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
"                fragColor = texture2D(image0, uv);\n"
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
"            imageColor = texture2D(image0, uv);\n"
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

void ImageRendererCreate(ImageRenderer &renderer, int width, int height,
                         unsigned char *image)
{
    OpenGLCHK(glGenTextures(1, &renderer.texture));
    OpenGLCHK(glBindTexture(GL_TEXTURE_2D, renderer.texture));

    OpenGLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    OpenGLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    OpenGLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    OpenGLCHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    OpenGLCHK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width,
                           height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image));
    renderer.Inited();
}

void ImageRendererCleanup(ImageRenderer &renderer){
    if(renderer.IsInited()){
        glDeleteTextures(1, &renderer.texture);
        renderer.texture = 0;
        renderer.inited = false;
    }
}

void ImageRendererUpdate(ImageRenderer &renderer, int width, int height,
                         unsigned char *image)
{
    if(!renderer.IsInited())
        return;

    OpenGLCHK(glBindTexture(GL_TEXTURE_2D, renderer.texture));
    OpenGLCHK(glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                        width, height, GL_RGBA, GL_UNSIGNED_BYTE, image));
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
    OpenGLCHK(glBindTexture(GL_TEXTURE_2D, renderer.texture));
    Shader_UniformInteger(shaderImg, "image0", 0);
    Shader_UniformFloat(shaderImg, "width", geometry->Width());
    Shader_UniformFloat(shaderImg, "height", geometry->Height());
    Shader_UniformVec2(shaderImg, "zoomCenter", zoomCenter);
    Shader_UniformFloat(shaderImg, "zoomLevel", zoomLevel);
    Shader_UniformVec4(shaderImg, "backgroundColor", backgroundColor);

    OpenGLCHK(glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, 1));
#if 1
    glUseProgram(shaderBorder.id);
    Shader_UniformVec4(shaderBorder, "backgroundColor", backgroundColor);
    Shader_UniformFloat(shaderBorder, "transitionFactor", factor);

    OpenGLCHK(glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, 1));
#endif
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glUseProgram(0);
}
