//
// Copyright (c) 2009-2013 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//	claim that you wrote the original software. If you use this software
//	in a product, an acknowledgment in the product documentation would be
//	appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//	misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

// Modified 16/04/2021 by Felipe: The code was ok, but it was leaking a lot of resources
//                                added fixed gpu buffer manipulation for better performance
//                                and memory management in order to make it stable.
#ifndef GL3COREFONTSTASH_H
#define GL3COREFONTSTASH_H
#include <string.h>
#include <geometry.h>
#include <types.h>

#if !defined(FONS_VERTEX_COUNT)
#define FONS_VERTEX_COUNT 8192
#endif

#ifdef __cplusplus
extern "C" {
#endif

    FONS_DEF FONScontext* glfonsCreate(int width, int height, int flags);
    FONS_DEF FONScontext* glfonsCreateFrom(FONScontext *other);
    FONS_DEF void glfonsDelete(FONScontext* ctx);

    /*FONS_DEF */unsigned int glfonsRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a);

#ifdef __cplusplus
}
#endif

#endif // GL3COREFONTSTASH_H

#if defined(GLFONTSTASH_IMPLEMENTATION) || defined(GLFONTSTASH_IMPLEMENTATION_ES2)


#ifndef GLFONS_VERTEX_ATTRIB
#	define GLFONS_VERTEX_ATTRIB 0
#endif

#ifndef GLFONS_TCOORD_ATTRIB
#	define GLFONS_TCOORD_ATTRIB 1
#endif

#ifndef GLFONS_COLOR_ATTRIB
#	define GLFONS_COLOR_ATTRIB 2
#endif

struct GLFONScontext {
	GLuint tex;
	int width, height;
	GLuint vertexBuffer;
	GLuint tcoordBuffer;
	GLuint colorBuffer;
	GLuint vertexArray; // Not used if GLFONTSTASH_IMPLEMENTATION_ES2 is defined
};
typedef struct GLFONScontext GLFONScontext;

static int glfons__renderCreateFrom(void *userPtr, void *otherPtr){
    GLFONScontext* gl  = (GLFONScontext*)userPtr;
    GLFONScontext* ogl = (GLFONScontext*)otherPtr;

    if(gl->tex != 0){
        glDeleteTextures(1, &gl->tex);
        gl->tex = 0;
    }

    gl->tex = ogl->tex;
    gl->vertexBuffer = ogl->vertexBuffer;
    gl->tcoordBuffer = ogl->tcoordBuffer;
    gl->colorBuffer  = ogl->colorBuffer;
    gl->width = ogl->width;
    gl->height = ogl->height;
    glGenVertexArrays(1, &gl->vertexArray);
    return 1;
}

static int glfons__renderCreate(void* userPtr, int width, int height)
{
    GLFONScontext* gl = (GLFONScontext*)userPtr;

    // Create may be called multiple times, delete existing texture.
    if (gl->tex != 0) {
        glDeleteTextures(1, &gl->tex);
        gl->tex = 0;
    }

    glGenTextures(1, &gl->tex);
    if (!gl->tex) return 0;

    // Only use VAO if they are supported. This way the same implementation works on OpenGL ES2 too.
#ifndef GLFONTSTASH_IMPLEMENTATION_ES2
    if (!gl->vertexArray) glGenVertexArrays(1, &gl->vertexArray);
    if (!gl->vertexArray) return 0;

    glBindVertexArray(gl->vertexArray);
#endif

    if (!gl->vertexBuffer){
        GL_CHK(glGenBuffers(1, &gl->vertexBuffer));
        GL_CHK(glBindBuffer(GL_ARRAY_BUFFER, gl->vertexBuffer));
        GL_CHK(glBufferData(GL_ARRAY_BUFFER, FONS_VERTEX_COUNT * 2 * sizeof(float),
        NULL, GL_DYNAMIC_DRAW));
        GL_CHK(glBindBuffer(GL_ARRAY_BUFFER, 0));
    }

    if (!gl->vertexBuffer) return 0;

    if (!gl->tcoordBuffer){
        GL_CHK(glGenBuffers(1, &gl->tcoordBuffer));
        GL_CHK(glBindBuffer(GL_ARRAY_BUFFER, gl->tcoordBuffer));
        GL_CHK(glBufferData(GL_ARRAY_BUFFER, FONS_VERTEX_COUNT * 2 * sizeof(float),
        NULL, GL_DYNAMIC_DRAW));
        GL_CHK(glBindBuffer(GL_ARRAY_BUFFER, 0));
    }
    if (!gl->tcoordBuffer) return 0;

    if (!gl->colorBuffer){
        GL_CHK(glGenBuffers(1, &gl->colorBuffer));
        GL_CHK(glBindBuffer(GL_ARRAY_BUFFER, gl->colorBuffer));
        GL_CHK(glBufferData(GL_ARRAY_BUFFER, FONS_VERTEX_COUNT * sizeof(unsigned int),
        NULL, GL_DYNAMIC_DRAW));
        GL_CHK(glBindBuffer(GL_ARRAY_BUFFER, 0));
    }
    if (!gl->colorBuffer) return 0;

    gl->width = width;
    gl->height = height;
    GL_CHK(glBindTexture(GL_TEXTURE_2D, gl->tex));

#ifdef GLFONTSTASH_IMPLEMENTATION_ES2
    GL_CHK(glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, gl->width, gl->height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, NULL));
    GL_CHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
#else
    GL_CHK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, gl->width, gl->height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL));
    GL_CHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GL_CHK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    static GLint swizzleRgbaParams[4] = {GL_ONE, GL_ONE, GL_ONE, GL_RED};
    GL_CHK(glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleRgbaParams));
#endif

    return 1;
}

static int glfons__renderResize(void* userPtr, int width, int height)
{
	// Reuse create to resize too.
	return glfons__renderCreate(userPtr, width, height);
}

static void glfons__renderUpdate(void* userPtr, int* rect, const unsigned char* data)
{
	GLFONScontext* gl = (GLFONScontext*)userPtr;

#ifdef GLFONTSTASH_IMPLEMENTATION_ES2
	GLint alignment;
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
	GL_CHK(glBindTexture(GL_TEXTURE_2D, gl->tex));
	GL_CHK(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
	// TODO: for now the whole texture is updated every time. Profile how bad is this.
	// as ES2 doesn't seem to support GL_UNPACK_ROW_LENGTH the only other option is to make a temp copy of the updated
	// portion with contiguous memory which is probably even worse.
	GL_CHK(glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, gl->width, gl->height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, data));
	GL_CHK(glPixelStorei(GL_UNPACK_ALIGNMENT, alignment));

#else
	// OpenGl desktop/es3 should use this version:
	int w = rect[2] - rect[0];
	int h = rect[3] - rect[1];

	if (gl->tex == 0) return;

	// Push old values
	GLint alignment, rowLength, skipPixels, skipRows;
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
	glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rowLength);
	glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skipPixels);
	glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skipRows);

	glBindTexture(GL_TEXTURE_2D, gl->tex);

	GL_CHK(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    GL_CHK(glPixelStorei(GL_UNPACK_ROW_LENGTH, gl->width));
    GL_CHK(glPixelStorei(GL_UNPACK_SKIP_PIXELS, rect[0]));
    GL_CHK(glPixelStorei(GL_UNPACK_SKIP_ROWS, rect[1]));

	GL_CHK(glTexSubImage2D(GL_TEXTURE_2D, 0, rect[0], rect[1], w,
                           h, GL_RED, GL_UNSIGNED_BYTE, data));

	// Pop old values
	GL_CHK(glPixelStorei(GL_UNPACK_ALIGNMENT, alignment));
    GL_CHK(glPixelStorei(GL_UNPACK_ROW_LENGTH, rowLength));
    GL_CHK(glPixelStorei(GL_UNPACK_SKIP_PIXELS, skipPixels));
    GL_CHK(glPixelStorei(GL_UNPACK_SKIP_ROWS, skipRows));
#endif

}

static void glfons__renderDraw(void* userPtr, const float* verts, const float* tcoords, const unsigned int* colors, int nverts)
{
	GLFONScontext* gl = (GLFONScontext*)userPtr;
    long int vertLen  = nverts * 2 * sizeof(float);
    long int colorLen = nverts * sizeof(unsigned int);

    if(nverts >= FONS_VERTEX_COUNT){
        printf("Attempting to render more than estimated maximum %d\n", nverts);
        getchar();
    }

#ifdef GLFONTSTASH_IMPLEMENTATION_ES2
	if (gl->tex == 0) return;
#else
	if (gl->tex == 0 || gl->vertexArray == 0) return;

	glBindVertexArray(gl->vertexArray);
#endif

    //printf("Nverts: %d\n", nverts);
#if defined(MEMORY_DEBUG)
    //__gpu_gl_get_memory_usage();
#endif

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gl->tex);

	glEnableVertexAttribArray(GLFONS_VERTEX_ATTRIB);
	GL_CHK(glBindBuffer(GL_ARRAY_BUFFER, gl->vertexBuffer));
    GL_CHK_BINDING(gl->vertexBuffer, vertLen);

    GL_CHK(glBufferSubData(GL_ARRAY_BUFFER, 0, vertLen, verts));
    GL_CHK(glVertexAttribPointer(GLFONS_VERTEX_ATTRIB, 2, GL_FLOAT, GL_FALSE, 0, NULL));

	glEnableVertexAttribArray(GLFONS_TCOORD_ATTRIB);
	GL_CHK(glBindBuffer(GL_ARRAY_BUFFER, gl->tcoordBuffer));
    GL_CHK_BINDING(gl->tcoordBuffer, vertLen);

    GL_CHK(glBufferSubData(GL_ARRAY_BUFFER, 0, vertLen, tcoords));
    GL_CHK(glVertexAttribPointer(GLFONS_TCOORD_ATTRIB, 2, GL_FLOAT, GL_FALSE, 0, NULL));

	glEnableVertexAttribArray(GLFONS_COLOR_ATTRIB);
	GL_CHK(glBindBuffer(GL_ARRAY_BUFFER, gl->colorBuffer));
    GL_CHK_BINDING(gl->colorBuffer, colorLen);

    GL_CHK(glBufferSubData(GL_ARRAY_BUFFER, 0, colorLen, colors));
    GL_CHK(glVertexAttribPointer(GLFONS_COLOR_ATTRIB, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, NULL));

	GL_CHK(glDrawArrays(GL_TRIANGLES, 0, nverts));

	glDisableVertexAttribArray(GLFONS_VERTEX_ATTRIB);
	glDisableVertexAttribArray(GLFONS_TCOORD_ATTRIB);
	glDisableVertexAttribArray(GLFONS_COLOR_ATTRIB);

#ifndef GLFONTSTASH_IMPLEMENTATION_ES2
	glBindVertexArray(0);
#endif
}

static void glfons__renderDelete(void* userPtr)
{
    /// ???????
	GLFONScontext* gl = (GLFONScontext*)userPtr;
	if (gl->tex != 0) {
		glDeleteTextures(1, &gl->tex);
		gl->tex = 0;
	}

    printf("Delete call!\n");
    return;
#ifndef GLFONTSTASH_IMPLEMENTATION_ES2
	glBindVertexArray(0);
#endif

	if (gl->vertexBuffer != 0) {
		glDeleteBuffers(1, &gl->vertexBuffer);
		gl->vertexBuffer = 0;
	}

	if (gl->tcoordBuffer != 0) {
		glDeleteBuffers(1, &gl->tcoordBuffer);
		gl->tcoordBuffer = 0;
	}

	if (gl->colorBuffer != 0) {
		glDeleteBuffers(1, &gl->colorBuffer);
		gl->colorBuffer = 0;
	}

#ifndef GLFONTSTASH_IMPLEMENTATION_ES2
	if (gl->vertexArray != 0) {
		glDeleteVertexArrays(1, &gl->vertexArray);
		gl->vertexArray = 0;
	}
#endif

	free(gl);
}


// TODO: Note this is creating a new context that shared GPU memory
//       it does not share the data in the context. So things like
//       fonts and colors are still separated.
FONS_DEF FONScontext* glfonsCreateFrom(FONScontext* other){
    FONSparams params;
    GLFONScontext* gl;
    gl = (GLFONScontext*)malloc(sizeof(GLFONScontext));
    if (gl == NULL) goto error;
    memset(gl, 0, sizeof(GLFONScontext));

    memset(&params, 0, sizeof(params));
    params.width = other->params.width;
    params.height = other->params.height;
    params.flags = (unsigned char)other->params.flags;
    params.renderCreate = NULL;
    params.renderResize = glfons__renderResize;
    params.renderUpdate = glfons__renderUpdate;
    params.renderDraw = glfons__renderDraw;
    params.renderDelete = glfons__renderDelete;
    params.userPtr = gl;

    glfons__renderCreateFrom((void *)gl, other->params.userPtr);
    return fonsCreateInternalFrom(&params, other);
error:
    return NULL;
}

FONS_DEF FONScontext* glfonsCreate(int width, int height, int flags)
{
	FONSparams params;
	GLFONScontext* gl;

	gl = (GLFONScontext*)malloc(sizeof(GLFONScontext));
	if (gl == NULL) goto error;
	memset(gl, 0, sizeof(GLFONScontext));

	memset(&params, 0, sizeof(params));
	params.width = width;
	params.height = height;
	params.flags = (unsigned char)flags;
	params.renderCreate = glfons__renderCreate;
	params.renderResize = glfons__renderResize;
	params.renderUpdate = glfons__renderUpdate;
	params.renderDraw = glfons__renderDraw;
	params.renderDelete = glfons__renderDelete;
	params.userPtr = gl;

	return fonsCreateInternal(&params);

error:
	if (gl != NULL) free(gl);
	return NULL;
}

FONS_DEF void glfonsDelete(FONScontext* ctx)
{
	fonsDeleteInternal(ctx);
}

FONS_DEF unsigned int glfonsRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	return (r) | (g << 8) | (b << 16) | (a << 24);
}

#endif // GLFONTSTASH_IMPLEMENTATION
