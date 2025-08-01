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
#include <display.h>
#include <map>
#include <file_provider.h>
#include <functional>
#include <vector>
#include <memory>
#include <parallel.h>
#include <encoding.h>
#include <atomic>

//TODO: Fix this, this might be too few texture units
#define MAX_TEXTURES_COUNT 256
#define MAX_TEXTURES_PER_BATCH 16

// look if you are trying to bind something that requires higher frequency
// than 1/1024, I'm sorry, you should review your event.
#define MIN_EVENT_SAMPLING_INTERVAL 0.000976562

#if !defined(_WIN32)
    #define FONT_UPSCALE_DEFAULT_SIZE 65
    #define FONT_UPSCALE_DEFAULT_OFFSET 47
#else
    #define FONT_UPSCALE_DEFAULT_SIZE 25
    #define FONT_UPSCALE_DEFAULT_OFFSET 10
#endif

#if 1
    #define OpenGLCHK(fn) do{\
        OpenGLClearErrors();\
        (fn);\
        OpenGLValidateErrors(#fn, __LINE__, __FILE__);\
    }while(0)

    #define OpenGLCHK_ERR() do{\
        OpenGLValidateErrors("ERR_CHK", __LINE__, __FILE__);\
    }while(0)
#else
    #define OpenGLCHK(fn) fn
#endif

struct View;
class WidgetWindow;
class DbgWidgetButtons;
class DbgWidgetExpressionViewer;

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
    Float referenceSize;
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
    int width;
    int height;
};

struct EventHandler{
    std::function<bool(void)> fn;
    double interval;
    double lastCalled;
};

// enables/disables rendering stuff
struct RenderProps{
    // renders the segment that connects cursor to ghost cursor
    bool cursorSegments;
};

struct Mouse{
    vec2ui position;
    bool isPressed;
};

// TODO: Widgets that are global to the editor
struct GlobalWidgets{
    WidgetWindow *wwindow;
    DbgWidgetButtons *wdbgBt;
    DbgWidgetExpressionViewer *wdbgVw;
};

struct OpenGLState{
    DisplayWindow *window;
    RenderProps params;
    OpenGLFont font;
    OpenGLBuffer glQuadBuffer;
    OpenGLImageQuadBuffer glQuadImageBuffer;
    OpenGLBuffer glLineBuffer;
    OpenGLCursor glCursor, glGhostCursor;
    OpenGLTexture textures[MAX_TEXTURES_COUNT];
    uint texBinds;
    Shader imageShader;
    Shader imRendererShader;
    Shader imBorderShader;
    Mouse mouse;
    double cursorBlinkLastTime;
    bool cursorBlinking;
    bool cursorVisible;
    bool enableCursorBlink;
    uint cursorBlinkKeyboardHandle;
    uint cursorBlinkMouseHandle;
    int running, width, height;
    Float renderLineWidth;
    Transform projection;
    Transform scale;
    Transform model;
    Transform imageModel;
    std::vector<EventHandler> eventHandlers;
    double eventInterval;
    double lastEventTime;
    double maxSamplingRate;
    Shader buttonShader;
    GlobalWidgets gWidgets;
    std::atomic<int> isCursorDisplayEventRunning;
    double cursorDisplayTime;

    // NOTE: We use shared_ptr here because otherwise we would need cyclic
    // dependencies because the signal for closing a widget goes to a widget
    // but it needs to report to the global state, so we would get a cyclic
    // call where widgets(onClose) -> state(remove) -> wigets(delete) which
    // it is not very good as onClose would return to a null object. Note
    // however that using shared_ptr will cause this anyways but at least we
    // don't actually need to explicitly handle. While it is kinda ugly it
    // does work really well.
    std::vector<std::shared_ptr<WidgetWindow>> widgetWindows;

    std::vector<BuildError> bErrors;

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

// expose the downscale flag
extern int useDriverDownscale;

void Graphics_Initialize();
int  Graphics_IsRunning();

/*
* Initializes a texture.
*/
void Graphics_TextureInit(OpenGLState *state, const char *path,
                          const char *key, FileExtension type = FILE_EXTENSION_NONE);
void Graphics_TextureInit(OpenGLState *state, uint8 *data, uint len,
                          const char *key, FileExtension type = FILE_EXTENSION_NONE);


DisplayWindow *Graphics_GetGlobalWindow();

/*
* Toogles the rendering of the cursor segments.
*/
void Graphics_ToogleCursorSegment();

/*
* Toogles the events and rendering of cursor blinking.
*/
void Graphics_ToogleCursorBlinking();

/*
* Initializes a openglbuffer to the current binding context.
*/
void OpenGLBufferInitialize(OpenGLBuffer *buffer, int n);
void OpenGLBufferInitializeFrom(OpenGLBuffer *buffer, OpenGLBuffer *other);
void OpenGLImageBufferInitializeFrom(OpenGLImageQuadBuffer *buffer,
                                     OpenGLImageQuadBuffer *other);

/*
* Computes the orthographic projection from the rectangle given by lower and upper.
*/
void OpenGLComputeProjection(vec2ui lower, vec2ui upper, Transform *transform);

/*
* Computes the viweing geometry of the cursor.
*/
void OpenGLComputeCursor(OpenGLFont *font, OpenGLCursor *glCursor, const char *buffer,
                         uint len, Float x, Float cursorOffset, Float baseHeight,
                         EncoderDecoder *encoder);

/*
* Deletes the context-aware information from the OpenGLBuffer. This does not
* delete storage information, only OpenGL related data that cannot be shared
* i.e.: vertex arrays, ...
*/
void OpenGLBufferContextDelete(OpenGLBuffer *buffer);
void OpenGLBufferContextDelete(OpenGLImageQuadBuffer *buffer);

void OpenGLFontCopy(OpenGLFont *dst, OpenGLFont *src);

/*
* Get the id of a texture for a given file or given by a string.
*/
uint Graphics_FetchTextureFor(OpenGLState *state, FileEntry *e, int *off);
uint Graphics_FetchTextureFor(OpenGLState *state, FileExtension type, int *off);

/*
* Get all information from a texture given its id.
*/
OpenGLTexture Graphics_GetTextureInfo(uint id);

/*
* Pushes a new quad into the current OpenGLState/OpenGLBuffer quad batch to render.
*/
void Graphics_QuadPush(OpenGLState *state, vec2ui left, vec2ui right, vec4f color);
void Graphics_QuadPush(OpenGLBuffer *quadB, vec2ui left, vec2ui right, vec4f color);

/*
* Retrieves the handler of the global (main) window.
*/
void *Graphics_GetBaseWindow();

/*
* Retrieves the handler for the global (main) widget window.
*/
WidgetWindow *Graphics_GetBaseWidgetWindow();

/*
* Retrieves the dbg widget components.
*/
DbgWidgetButtons *Graphics_GetDbgWidget();
DbgWidgetExpressionViewer *Graphics_GetDbgExpressionViewer();

/*
* Retrieves the global gl context.
*/
OpenGLState *Graphics_GetGlobalContext();

/*
* Removes a widgetwindow from the global widgetinwdow list.
*/
void Graphics_RequestClose(WidgetWindow *w);

/*
* Sets the font to the given ttf.
*/
void Graphics_SetFont(char *ttf, uint len);

/*
* Pushes a new quad into the current OpenGLState image batch to render a image.
* Returns whether or not it was possible to render, i.e.: maximum amount of
* textures was reached and should be flushed. Optinally can also use the image
* buffer directly in case it is being shared.
*/
int Graphics_ImagePush(OpenGLState *state, vec2ui left, vec2ui right, int mid);
int Graphics_ImagePush(OpenGLImageQuadBuffer *quad, vec2ui left, vec2ui right, int mid);

/*
* Pushes a quad border into the quad batch buffer.
*/
void Graphics_QuadPushBorder(OpenGLState *state, Float x0, Float y0,
                             Float x1, Float y1, Float w, vec4f col);
void Graphics_QuadPushBorder(OpenGLBuffer *quadB, Float x0, Float y0,
                             Float x1, Float y1, Float w, vec4f col);

/*
* Pushes a new line into the current OpenGLState line batch to render.
*/
void Graphics_LinePush(OpenGLState *state, vec2ui p0, vec2ui p1, vec4f color);

/*
* Triggers the OpenGLState/OpenGLBuffer quad accumulated batch to render all quads pushed.
*/
void Graphics_QuadFlush(OpenGLState *state, int blend=1);
void Graphics_QuadFlush(OpenGLBuffer *quad, int blend=1);

/*
* Triggers the OpenGLState line accumulated batch to render all lines pushed.
*/
void Graphics_LineFlush(OpenGLState *state, int blend=1);

/*
* Triggers the OpenGLState image buffer accumulated batch to render all images.
*/
void Graphics_ImageFlush(OpenGLState *state, OpenGLImageQuadBuffer *quad=nullptr);

/*
* Gets the font size being used by the global renderer.
*/
uint Graphics_GetFontSize();

/*
* Sets the font size being used.
* Warning: Be carefull with viewports and projections.
*/
void Graphics_SetFontSize(OpenGLState *state, Float fontSize,
                          Float reference=FONT_UPSCALE_DEFAULT_SIZE);

/*
* Sets the default font size, this routine updates the global OpenGLState, be aware that
* changing these will take effect on all components. It is a convenience for the
* fontsize command to allow for global changes.
*/
void Graphics_SetDefaultFontSize(uint fontSize);

/*
* Consultes the global OpenGLState for the current line height being used.
*/
uint Graphics_GetDefaultLineHeight();

/*
* Computes the transformation required to apply the given fontsize. This routines also
* adjusts the values inside the given font to be compatible with the transformation
* but does NOT change the inner transforms of any OpenGLState.
*/
void Graphics_ComputeTransformsForFontSize(OpenGLFont *font, Float fontSize, Transform *model,
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
                            uint upto, int &previousGlyph, EncoderDecoder *encoder);

/*
* Returns the size a token takes when rendering.
*/
Float Graphics_GetTokenXSize(OpenGLState *state, Buffer *buffer, uint at,
                             int &previousGlyph, EncoderDecoder *encoder);

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
* Renders the cursor at a specific location within the current bounded window
* into the quad of size (x0, y0) x (x1, y1). This routine queries the editor
* state to check the cursor format.
*/
void Graphics_RenderCursorAt(OpenGLBuffer *quad, Float x0, Float y0, Float x1,
                             Float y1, vec4f color, uint thickness, int isActive);

/*
* Computes the color of the editing cursor.
*/
vec4f Graphics_GetCursorColor(BufferView *view, Theme *theme, int ghost=0);

/*
* Renders a BufferView, returns != 0 in case the bufferview is animating and cannot
* be put on hold.
*/
int Graphics_RenderBufferView(View *view, OpenGLState *state,
                              Theme *theme, Float dt, int detached_running);

/*
* Renders a welcome message in the given view.
*/
int Graphics_RenderDefaultView(View *view, OpenGLState *state, Theme *theme, Float dt);

/*
* Generic call to render a view, it basically checks if the view has a file to render
* otherwise it renders a default welcome message, render files as text to be edited.
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
* Renders the a view that contains an image.
*/
int Graphics_RenderImage(View *view, OpenGLState *state, Theme *theme, Float dt);

/*
* Given a view and a target viewport, prepares the viewport and scissor
* geometry to perform rendering, also updates the OpenGLState given with the
* target projection required to performed rendering.
*/
void ActivateViewportAndProjection(OpenGLState *state, View *view, GLViewport target);

/*
* Sets the active viewport and scissor geometry to the specified geometry, it also
* updates the target projection for rendering in this viewport. The value of
* 'with_scissors' can be used to activate (or not) scissors in the viewport
* that is currently being activated.
*/
void ActivateViewportAndProjection(OpenGLState *state, Geometry *geometry,
                                   bool with_scissors=true);

/*
* Prepares shaders to render text.
*/
void Graphics_PrepareTextRendering(OpenGLState *state, Transform *projection,
                                   Transform *model);
void Graphics_PrepareTextRendering(OpenGLFont *font, Transform *projection,
                                   Transform *model);

/*
* Batches text to perform rendering.
*/
void Graphics_PushText(OpenGLState *state, Float &x, Float &y, char *text,
                       uint len, vec4i col, int *pGlyph, EncoderDecoder *encoder);
void Graphics_PushText(OpenGLFont *font, Float &x, Float &y, char *text,
                       uint len, vec4i col, int *pGlyph, EncoderDecoder *encoder);

/*
* Triggers a render call for all batched texts pushed.
*/
void Graphics_FlushText(OpenGLState *state);
void Graphics_FlushText(OpenGLFont *font);

/*
* TODO
*/
vec2i Graphics_ComputeSelectableListItem(OpenGLState *state, uint y, View *view);

/*
* Computes the coordinates to start rendering the 'text' with size 'len' using 'font'
* inside 'geometry' such that the text be centered. The variable 'in_place' can be used
* if the rendering is considering the lower = vec2ui(0) and the geometry itself should
* be used to compute the center against [0,0] [w,h].
*/
vec2f Graphics_ComputeCenteringStart(OpenGLFont *font, const char *text,
                                     uint len, Geometry *geometry, bool in_place,
                                     EncoderDecoder *encoder);

/*
* Bind all image units in the quad image buffer using the global rendering state.
*/
void Graphics_BindImages(OpenGLState *state, OpenGLImageQuadBuffer *quad);

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
* might not be called in 'ival' seconds, it is however garanteed to be executed by a
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
vec2f ScreenToGLf(vec2f u, OpenGLState * state);
Float GLToScreen(Float x, OpenGLState *state);
vec2f GLToScreen(vec2f u, OpenGLState *state);
Float ScreenToTransform(Float x, Transform model);

/*
* Debug utilities.
*/
void OpenGLClearErrors();
void OpenGLValidateErrors(const char *fn, int line, const char *file);

#endif //GRAPHICS_H
