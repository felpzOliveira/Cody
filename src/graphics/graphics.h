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
#include <map>
#include <file_provider.h>
#include <functional>

//TODO: Fix this, this might be too few texture units
#define MAX_TEXTURES_COUNT 256
#define MAX_TEXTURES_PER_BATCH 16

// look if you are trying to bind something that requires higher frequency
// than 1/1024, I'm sorry, you should review your event.
#define MIN_EVENT_SAMPLING_INTERVAL 0.000976562

// Our OpenGL rendering renders everything in very high resolution and
// than reduce scaling to get simple AA. This could potentially be slow
// but it is working fine for me.
#define FONT_UPSCALE_DEFAULT_SIZE 65
#define FONT_UPSCALE_DEFAULT_OFFSET 47

#if defined(DEBUG_BUILD)
#define OpenGLCHK(fn) do{\
    OpenGLClearErrors();\
    (fn);\
    OpenGLValidateErrors(#fn, __LINE__, __FILE__);\
}while(0)
#else
#define OpenGLCHK(fn) fn
#endif

struct View;

/*
* OpenGL rendering system for the editor.
*/

typedef uint GlTextureId;

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

struct OpenGLImageQuadBuffer{
    // Data
    Float *vertex;
    Float *tex;
    uint size;
    uint length;
    //OpenGL
    uint vertexArray;
    uint vertexBuffer;
    uint texBuffer;
    uint textureIds[MAX_TEXTURES_PER_BATCH];
    uint units;
};

struct OpenGLTexture{
    uint textureId;
    int format;
};

struct EventHandler{
    std::function<bool(void)> fn;
    double interval;
};

struct OpenGLState{
    WindowX11 *window;
    OpenGLFont font;
    OpenGLBuffer glQuadBuffer;
    OpenGLImageQuadBuffer glQuadImageBuffer;
    OpenGLBuffer glLineBuffer;
    OpenGLCursor glCursor, glGhostCursor;
    OpenGLTexture textures[MAX_TEXTURES_COUNT];
    uint texBinds;
    Shader imageShader;
    vec2ui mouse;
    int running, width, height;
    Float renderLineWidth;
    Bounds2f windowBounds;
    Transform projection;
    Transform scale;
    Transform model;
    std::vector<EventHandler> eventHandlers;
    double eventInterval;
    // TODO: Maybe don't use maps
    std::map<std::string, uint> textureMap;
    std::map<FileExtension, std::string> fileMap;
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
* Initializes a texture.
*/
void Graphics_TextureInit(OpenGLState *state, const char *path,
                          const char *key, FileExtension type = FILE_EXTENSION_NONE);
void Graphics_TextureInit(OpenGLState *state, uint8 *data, uint len,
                          const char *key, FileExtension type = FILE_EXTENSION_NONE);

/*
* Get the id of a texture for a given file.
*/
uint Graphics_FetchTextureFor(OpenGLState *state, FileEntry *e, int *off);
uint Graphics_FetchTextureFor(OpenGLState *state, FileExtension type, int *off);

/*
* Pushes a new quad into the current OpenGLState quad batch to render.
*/
void Graphics_QuadPush(OpenGLState *state, vec2ui left, vec2ui right, vec4f color);

/*
* Pushes a new quad into the current OpenGLState image batch to render a image.
* Returns whether or not it was possible to render, i.e.: maximum amount of
* textures was reached and should be flushed.
*/
int Graphics_ImagePush(OpenGLState *state, vec2ui left, vec2ui right, int mid);

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
* Triggers the OpenGLState image buffer accumulated batch to render all images.
*/
void Graphics_ImageFlush(OpenGLState *state, int reset=1);

/*
* Sets the font size being used.
* Warning: Be carefull with viewports and projections.
*/
void Graphics_SetFontSize(OpenGLState *state, Float fontSize,
                          Float reference=FONT_UPSCALE_DEFAULT_SIZE);

/*
* Queries the opengl state for the last known cursor position.
*/
vec2ui Graphics_GetMouse(OpenGLState *state);

/*
* Computes the vertical position of a given line 'i' that is within the range 'visible'
* inside 'view'. Return value is are the coordinates vec2f(start, end) of the line
* with end = start + font->fontMath.fontSizeAtRenderCall.
*/
vec2f Graphics_GetLineYPos(OpenGLState *state, vec2ui visible, uint i, View *view);

/*
* Computes the horizontal position of the start of the token index 'upto' in the
* given buffer.
*/
Float Graphics_GetTokenXPos(OpenGLState *state, Buffer *buffer,
                            uint upto, int &previousGlyph);

/*
* Returns the size a token takes when rendering.
*/
Float Graphics_GetTokenXSize(OpenGLState *state, Buffer *buffer, uint at,
                             int &previousGlyph);

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
* Renders a View's SelectableList.
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
* Renders anything related to the control commands interface.
*/
int Graphics_RenderControlCommands(View *view, OpenGLState *state,
                                   Theme *theme, Float dt);

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
* TODO
*/
vec2i Graphics_ComputeSelectableListItem(OpenGLState *state, uint y, View *view);

/*
* Bind all image units.
*/
void Graphics_BindImages(OpenGLState *state);

/*
* Add an event handling function into the rendering thread. This can be used by
* routines that want to do live update and require UI to not pause for events.
* The ival value tells what is the interval of sampling it should use and eH tells what
* function to call during the pooling procedure in order to handle the events. The
* handler should use the boolean return to tell if the routine needs to be sampled again,
* i.e.: returning false makes the routine be removed from the event handling routine list.
* Maximum sampling frequency is given by MIN_EVENT_SAMPLING_INTERVAL. Note that the
* interval given might not be respected. If any other event requires a higher sampling
* rate, than the routine will be sampled at higher frequency, this avoids races where
* it would not be possible to combine different events under different frequencies.
* Because all events run together, if an event take too long to return the routine
* might not be called in 'ival' seconds, it is however garanteed to be executed by an
* triggered timer under 'ival' seconds. Events are called *after* the rendering is done
* so the routine might consider all previous actions to UI have already been updated
* into the current framebuffer.
*/
void Graphics_AddEventHandler(double ival, std::function<bool(void)> eH);

/*
* Converts a screen coordinates to a rendering coordinate.
*/
Float ScreenToGL(Float x, OpenGLState *state);
vec2ui ScreenToGL(vec2ui u, OpenGLState * state);
Float GLToScreen(Float x, OpenGLState *state);

/*
* Debug utilities.
*/
void OpenGLClearErrors();
void OpenGLValidateErrors(const char *fn, int line, const char *file);

#endif //GRAPHICS_H
