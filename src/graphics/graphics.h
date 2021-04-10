/* date = January 10th 2021 10:12 am */

#ifndef GRAPHICS_H
#define GRAPHICS_H
#include <geometry.h>
#include <types.h>
#include <shaders.h>
#include <transform.h>
#include <bufferview.h>
#include <theme.h>
#include <fontstash.h>
#include <x11_display.h>

struct View;

/*
* OpenGL rendering system for the editor.
*/

typedef enum{
    ViewportLineNumbers,
    ViewportText,
    ViewportFrame,
    ViewportAllView
}GLViewport;

struct FontMath{
    Float fontSizeAtRenderCall;
    Float fontSizeAtDisplay;
    Float reduceScale;
    Float invReduceScale;
};

struct OpenGLFont{
    FONScontext *fsContext;
    FONSsdfSettings sdfSettings;
    Shader shader;
    Shader cursorShader;
    int fontId;
    FontMath fontMath;
};

struct OpenGLCursor{
    vec2f pMin;
    vec2f pMax;
    int valid;
    int pGlyph;
    vec2ui textPos;
    Float currentDif;
};

struct OpenGLBuffer{
    //Data stuff
    Float *vertex;
    Float *colors;
    uint size;
    uint length;
    //OpenGL stuff
    uint vertexArray;
    uint vertexBuffer;
    uint colorBuffer;
};

struct OpenGLState{
    WindowX11 *window;
    OpenGLFont font;
    OpenGLBuffer glQuadBuffer;
    OpenGLBuffer glLineBuffer;
    OpenGLCursor glCursor, glGhostCursor;
    vec2ui mouse;
    int running, width, height;
    Float renderLineWidth;
    Bounds2f windowBounds;
    Transform projection;
    Transform scale;
    Transform model;
};

#define RENDER_STAGE_CALL(name) int name(View *view, OpenGLState *state, Theme *theme, Float dt)
typedef RENDER_STAGE_CALL(graphics_render_call);

typedef struct{
    graphics_render_call *renderer;
}RenderStage;

typedef struct{
    RenderStage stages[8];
    uint stageCount;
}RenderList;


void Graphics_Initialize();
int  Graphics_IsRunning();

/*
* Pushes a new quad into the current OpenGLState quad batch to render.
*/
void Graphics_QuadPush(OpenGLState *state, vec2ui left, vec2ui right, vec4f color);

/*
* Pushes a quad border into the quad batch buffer.
*/
void Graphics_QuadPushBorder(OpenGLState *state, Float x0, Float y0,
                             Float x1, Float y1, Float w, vec4f col);

/*
* Pushes a new line into the current OpenGLState line batch to render.
*/
void Graphics_LinePush(OpenGLState *state, vec2ui p0, vec2ui p1, vec4f color);

/*
* Triggers the OpenGLState quad accumulated batch to render all quads pushed.
*/
void Graphics_QuadFlush(OpenGLState *state, int blend=1);

/*
* Triggers the OpenGLState line accumulated batch to render all lines pushed.
*/
void Graphics_LineFlush(OpenGLState *state, int blend=1);

/*
* Sets the font size being used.
* Warning: Be carefull with viewports and projections.
*/
void Graphics_SetFontSize(OpenGLState *state, Float fontSize);

/*
* Queries the opengl state for the last known cursor position.
*/
vec2ui Graphics_GetMouse(OpenGLState *state);

/*
* Computes the color a token should have according to the current given theme
* and text format. This function returns -1 in case the given token does not
* need to be rendered or 0 in case it needs, in which case the color is given
* in 'color'. 'bView', 'lineNr' and 'tid' are only required if correct
* braces color are expected for nesting points. Giving bView = nullptr
* skips nest computation.
*/
int Graphics_ComputeTokenColor(char *str, Token *token, SymbolTable *symTable,
                               Theme *theme, uint lineNr, uint tid,
                               BufferView *bView, vec4i *color);

/*
* Computes the color of the editing cursor.
*/
vec4f Graphics_GetCursorColor(BufferView *view, Theme *theme, int ghost=0);

/*
* Renders a BufferView, returns != 0 in case the bufferview is animating and cannot
* be put on hold.
*/
int Graphics_RenderBufferView(View *view, OpenGLState *state,
                              Theme *theme, Float dt);

/*
* Renders a welcome message in the given view.
*/
int Graphics_RenderDefaultView(View *view, OpenGLState *state, Theme *theme, Float dt);

/*
* Generic call to render a view, it basically checks if the view has a file to render
* otherwise it renders a default welcome message.
*/
int Graphics_RenderView(View *view, OpenGLState *state, Theme *theme, Float dt);
/*
* Renders a OpenGLSelector.
*/
int Graphics_RenderQueryBar(View *view, OpenGLState *state,
                            Theme *theme, Float dt);

/*
* Renders a SelectableList active in a view.
*/
int Graphics_RenderListSelector(View *view, OpenGLState *state,
                                Theme *theme, Float dt);

/*
* Renders a linebuffer as a hoverable component.
*/
int Graphics_RenderHoverableList(View *view, OpenGLState *state, Theme *theme,
                                 Geometry *geometry, LineBuffer *lineBuffer);

/*
* Renders the content of the autocomplete list in the current view.
*/
int Graphics_RenderAutoComplete(View *view, OpenGLState *state, Theme *theme, Float dt);

/*
* Given a view and a target viewport, prepares the viewport and scissor
* geometry to perform rendering, also updates the OpenGLState given with the
* target projection required to performed rendering.
*/
void ActivateViewportAndProjection(OpenGLState *state, View *view, GLViewport target);

/*
* Sets the active viewport and scissor geometry to the specified geometry, it also
* updates the target projection for rendering in this viewport.
*/
void ActivateViewportAndProjection(OpenGLState *state, Geometry *geometry);

/*
* Prepares shaders to render text.
*/
void Graphics_PrepareTextRendering(OpenGLState *state, Transform *projection,
                                   Transform *model);

/*
* Batches text to perform rendering.
*/
void Graphics_PushText(OpenGLState *state, Float &x, Float &y, char *text,
                       uint len, vec4i col, int *pGlyph);

/*
* Triggers a render call for all batched texts pushed.
*/
void Graphics_FlushText(OpenGLState *state);

/*
* Converts a screen coordinates to a rendering coordinate.
*/
Float ScreenToGL(Float x, OpenGLState *state);
vec2ui ScreenToGL(vec2ui u, OpenGLState * state);
Float GLToScreen(Float x, OpenGLState *state);

#endif //GRAPHICS_H
