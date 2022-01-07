#include <widgets.h>
#include <app.h>
#include <glad/glad.h>
#include <resources.h>
#include <theme.h>
#include <gl3corefontstash.h>
#include <timing.h>
#include <buffers.h>

uint ButtonWidget::kWidth  = 92;
uint ButtonWidget::kHeight = 32;
Float ButtonWidget::kScaleUp = 1.1;
Float ButtonWidget::kScaleDown = 0.9090909090909091;
Float ButtonWidget::kScaleDuration = 0.1;

#define CALLBACK_HANDLER(handler, x, y, priv, ...) do{\
    WidgetWindow *ww = (WidgetWindow *)priv;\
    vec2ui mouse(x, ww->resolution.y - y);\
    for(Widget *w : ww->widgetList){\
        if(Geometry_IsPointInside(&w->geometry, mouse)){\
            w->handler(__VA_ARGS__);\
            break;\
        }\
    }\
}while(0)

static void ActivateShaderAndProjections(Shader &shader, OpenGLState *state){
    OpenGLCHK(glUseProgram(shader.id));
    Shader_UniformMatrix4(shader, "projection", &state->projection.m);
    Shader_UniformMatrix4(shader, "modelView", &state->scale.m);
}

void WidgetWindowOnClick(int x, int y, void *priv){ CALLBACK_HANDLER(OnClick, x, y, priv); }
void WidgetWindowOnMotion(int x, int y, void *priv){
    // This works nicely but how is performance for doing this on motion?
    WidgetWindow *ww = (WidgetWindow *)priv;
    vec2ui mouse(x, ww->resolution.y - y);
    Widget *currWidget = nullptr;
    for(Widget *w : ww->widgetList){
        if(Geometry_IsPointInside(&w->geometry, mouse)){
            currWidget = w;
            break;
        }
    }

    if(currWidget == nullptr){
        // either we left a widget or we never entered one
        if(ww->lastWidget){
            ww->lastWidget->OnExited();
        }
    }else{
        // if we are inside the same widget than nothing happened
        if(currWidget == ww->lastWidget){
            // nothing
        }else if(ww->lastWidget){
            // if the widgets are different than they are really close
            // and we entered one and left the other
            ww->lastWidget->OnExited();
            currWidget->OnEntered();
        }else{
            // there is no last widget, we entered a new one
            currWidget->OnEntered();
        }
    }

    // update the last widget
    ww->lastWidget = currWidget;
}

WidgetWindow::WidgetWindow(){
    window = nullptr;
    mapping = nullptr;
}
WidgetWindow::WidgetWindow(int width, int height, const char *name){
    window = nullptr;
    mapping = nullptr;
    Open(width, height, name);
}

// TODO: maybe cache the keyboard mapping?
void WidgetWindow::Open(int width, int height, const char *name){
    if(window){
        DestroyWindowX11(window);
    }

    OpenGLState *state = Graphics_GetGlobalContext();
    WindowX11 *shared = (WindowX11 *)Graphics_GetBaseWindow();
    OpenGLFont *ofont = &state->font;
    window = CreateWindowX11Shared(width, height, name, shared);
    resolution = vec2f((Float)width, (Float)height);
    lastWidget = nullptr;
    SetNotResizable(window);
    SwapIntervalX11(window, 0);

    if(!mapping){
        MakeContext();
        mapping = KeyboardCreateMapping(window);
        KeyboardSetActiveMapping(mapping);
        RegisterOnMouseLeftClickCallback(window, WidgetWindowOnClick, this);
        RegisterOnMouseMotionCallback(window, WidgetWindowOnMotion, this);

        OpenGLFontCopy(&font, ofont);
    }else{
        OpenGLBufferContextDelete(&quadBuffer);
        OpenGLBufferContextDelete(&quadImageBuffer);
        glfonsDeleteContext(font.fsContext);
    }

    OpenGLBufferInitializeFrom(&quadBuffer, &state->glQuadBuffer);
    OpenGLImageBufferInitializeFrom(&quadImageBuffer, &state->glQuadImageBuffer);
    font.fsContext = glfonsCreateFrom(ofont->fsContext);
    if(font.fsContext == nullptr){
        printf("[Widget] Failed to create font locally\n");
    }

    // TODO: Set mapping callbacks
    // TODO: Cache mapping?
    // TODO: Register inputs
}

Geometry WidgetWindow::GetGeometry(){
    Geometry geo;
    geo.lower = vec2ui(0, 0);
    geo.upper = vec2ui(resolution.x, resolution.y);
    geo.extensionX = vec2f(0, 1);
    geo.extensionY = vec2f(0, 1);
    return geo;
}

void WidgetWindow::MakeContext(){
    if(window){
        MakeContextX11(window);
    }
}

int WidgetWindow::DispatchRender(WidgetRenderContext *wctx){
    int is_animating = 0;
    MakeContext();
    Geometry geometry = GetGeometry();
    vec4f background = DefaultGetUIColorf(UISelectorBackground);
    ActivateViewportAndProjection(wctx->state, &geometry);
    Float fcol[] = {background.x, background.y, background.z, background.w};

    glClearBufferfv(GL_COLOR, 0, fcol);
    glClearBufferfv(GL_DEPTH, 0, kOnes);

    // fill in window specifics
    wctx->windowBackground = background;
    wctx->window = this;

    for(Widget *w : widgetList){
        is_animating |= w->Render(wctx);
    }

    Update();
    return is_animating;
}

void WidgetWindow::Update(){
    if(window){
        if(!WindowShouldCloseX11(window)){
            SwapBuffersX11(window);
        }else{
            DestroyWindowX11(window);
            window = nullptr;
        }
    }
}

WidgetWindow::~WidgetWindow(){
    if(window){
        DestroyWindowX11(window);
    }
}

Widget::Widget(){
    transition.running = false;
    style.valid = false;
}
Widget::~Widget(){
    signalClicked.DisconnectAll();
}

void Widget::InterruptAnimation(){
    // simply simulate the maximum step
    geometry = AdvanceAnimation(transition.interval + Epsilon);
}

void Widget::Animate(const WidgetTransition &wtransition){
    if(!transition.running){
        Float w0 = geometry.upper.x - geometry.lower.x;
        Float h0 = geometry.upper.y - geometry.lower.y;
        transition.scale = Max(Epsilon, wtransition.scale);
        transition.startValues = vec2f(w0, h0);
        transition.endValues = vec2f(w0 * transition.scale, h0 * transition.scale);
        transition.type = wtransition.type;
        transition.interval = Max(Epsilon, wtransition.interval);
        transition.dt = 0;
        transition.velocity = vec2f(0);
        transition.running = true;
        Timing_Update();
    }
}

void Widget::SetGeometry(const Geometry &geo, vec2f aspectX, vec2f aspectY){
    Float w = geo.upper.x - geo.lower.x;
    Float h = geo.upper.y - geo.lower.y;
    uint x0 = (uint)(w * aspectX.x);
    uint x1 = (uint)(w * aspectX.y);
    uint y0 = (uint)(h * aspectY.x);
    uint y1 = (uint)(h * aspectY.y);

    geometry.lower = vec2ui(x0, y0);
    geometry.upper = vec2ui(x1, y1);
    geometry.extensionX = aspectX;
    geometry.extensionY = aspectY;
    refGeometry = geo;
}

Geometry Widget::AdvanceAnimation(Float dt){
    Geometry geo = geometry;
    if(!transition.running) return geometry;

    Float pw = refGeometry.upper.x - refGeometry.lower.x;
    Float ph = refGeometry.upper.y - refGeometry.lower.y;
    Float w0 = geo.upper.x - geo.lower.x;
    Float h0 = geo.upper.y - geo.lower.y;
    uint cx = (geo.upper.x + geo.lower.x) / 2;
    uint cy = (geo.upper.y + geo.lower.y) / 2;
    Float w02 = w0 / 2;
    Float h02 = h0 / 2;

    if(transition.type == WIDGET_TRANSITION_SCALE){
        Float x0 = 0, x1 = 0, y0 = 0, y1 = 0;
        Float wf = transition.endValues.x;
        Float hf = transition.endValues.y;
        Float wc = transition.startValues.x;
        Float hc = transition.startValues.y;

        Float vx = transition.velocity.x;
        Float vy = transition.velocity.y;
        Float remaining = transition.interval - transition.dt;
        Float w = InterpolateValueCubic(dt, remaining, &wc, wf, &vx);
        Float h = InterpolateValueCubic(dt, remaining, &hc, hf, &vy);

        transition.velocity = vec2f(vx, vy);
        transition.startValues = vec2f(w, h);

        int px = Absf((w - w0) / 2);
        int py = Absf((h - h0) / 2);
        if(w > w0){ // expanding
            x0 = Max(0, (int)cx - w02 - px);
            x1 = Max(0, (int)cx + w02 + px);
            y0 = Max(0, (int)cy - h02 - py);
            y1 = Max(0, (int)cy + h02 + py);
        }else{ // reducing
            x0 = Max(0, (int)cx - w02 + px);
            x1 = Max(0, (int)cx + w02 - px);
            y0 = Max(0, (int)cy - h02 + py);
            y1 = Max(0, (int)cy + h02 - py);
        }

        geo.lower = vec2ui(x0, y0);
        geo.upper = vec2ui(x1, y1);
        geo.extensionX = vec2f(x0 / pw, x1 / pw);
        geo.extensionY = vec2f(y0 / ph, y1 / ph);
    }
    else{
        // not yet supported
    }

    bool finish = !((transition.dt + dt) <= transition.interval);

    OnAnimationUpdate(dt, geo, finish);
    geometry = geo;
    transition.dt += dt;
    transition.running = !finish;

    return geo;
}

int Widget::Render(WidgetRenderContext *wctx){
    // Rendering of a widget is simple, we paint background
    // and call specifics to render itself
    int animating = transition.running ? 1 : 0;
    if(animating){
        geometry = AdvanceAnimation(wctx->dt);
    }
    ActivateViewportAndProjection(wctx->state, &geometry);
#if 0
    vec4f col = DefaultGetUIColorf(UISelectorBackground);
    Float fcol[] = {col.x, col.y, col.z, col.w};
    glClearBufferfv(GL_COLOR, 0, fcol);
#endif
    animating |= OnRender(wctx);

    glDisable(GL_SCISSOR_TEST);
    return animating;
}

void Widget::AddClickedSlot(Slot<> slot){
    signalClicked.Connect(slot);
}

ButtonWidget::ButtonWidget() : Widget(){
    baseFontSize = Graphics_GetFontSize();
    a_fontSize = (Float)baseFontSize;
    textureId = -1;
}
ButtonWidget::ButtonWidget(std::string val) : Widget(){
    SetText(val);
    baseFontSize = Graphics_GetFontSize();
    a_fontSize = (Float)baseFontSize;
    textureId = -1;
}

void ButtonWidget::PreferredGeometry(vec2f center, Geometry parentGeo){
    Float w = parentGeo.upper.x - parentGeo.lower.x;
    Float h = parentGeo.upper.y - parentGeo.lower.y;
    Float ux = w * center.x, uy = h * center.y;
    uint x0 = ux - ButtonWidget::kWidth / 2.0;
    uint x1 = ux + ButtonWidget::kWidth / 2.0;
    uint y0 = uy - ButtonWidget::kHeight / 2.0;
    uint y1 = uy + ButtonWidget::kHeight / 2.0;
    geometry.lower = vec2ui(x0, y0);
    geometry.upper = vec2ui(x1, y1);
    geometry.extensionX = vec2f((Float)x0 / w, (Float)x1 / w);
    geometry.extensionY = vec2f((Float)y0 / h, (Float)y1 / h);
    refGeometry = parentGeo;
}

void ButtonWidget::OnClick(){
    printf("Clicked button with color %g %g %g\n", style.background.x,
           style.background.y, style.background.z);
    Widget::OnClick();
}

void ButtonWidget::OnEntered(){
    WidgetTransition transition = {
        .type = WIDGET_TRANSITION_SCALE,
        .scale = ButtonWidget::kScaleUp,
        .interval = ButtonWidget::kScaleDuration,
        .dt = 0,
        .running =  true,
    };

    a_fontSize = (Float)baseFontSize;
    a_vel = 0;

    Widget::OnEntered();
    Widget::InterruptAnimation();
    Widget::Animate(transition);
}

void ButtonWidget::OnExited(){
    WidgetTransition transition = {
        .type = WIDGET_TRANSITION_SCALE,
        .scale = ButtonWidget::kScaleDown,
        .interval = ButtonWidget::kScaleDuration,
        .dt = 0,
        .running =  true,
    };

    a_fontSize = (Float)baseFontSize;
    a_vel = 0;

    Widget::OnExited();
    Widget::InterruptAnimation();
    Widget::Animate(transition);
}

void ButtonWidget::OnAnimationUpdate(Float dt, Geometry newGeometry, bool willFinish){
    if(transition.type == WIDGET_TRANSITION_SCALE){
        if(willFinish){
            // we need to make sure the ending point for the font
            // is *exactly* the scaling term otherwise float point
            // precision could kill the view on chained animation
            baseFontSize *= transition.scale;
            a_fontSize = (Float)baseFontSize;
        }else{
            Float remaining = transition.interval - transition.dt;
            Float fontSizeFinal = (Float)baseFontSize * transition.scale;
            InterpolateValueCubic(dt, remaining, &a_fontSize, fontSizeFinal, &a_vel);
        }
    }
}

int ButtonWidget::OnRender(WidgetRenderContext *wctx){
    OpenGLState *gl = wctx->state;
    WidgetWindow *parent = wctx->window;
    OpenGLFont *font = &parent->font;
    FontMath fMath = font->fontMath;
    Transform model;
    uint xoff = 0;

    Shader shader = gl->buttonShader;
    vec2f upper(geometry.upper.x - geometry.lower.x, geometry.upper.y - geometry.lower.y);
    upper *= font->fontMath.invReduceScale;

    ActivateShaderAndProjections(shader, gl);
    //Shader_UniformVec4(shader, "backgroundColor", wctx->windowBackground);
    //Shader_UniformVec2(shader, "buttonResolution", upper);

    Graphics_QuadPushBorder(&parent->quadBuffer, 0, 0, upper.x,
                            upper.y, 2.0, style.background);

    vec4f back = DefaultGetUIColorf(UISelectorBackground);
    back.w = 1.0;

    Graphics_QuadPush(&parent->quadBuffer, vec2ui(0,0), upper, back * 1.2);
    Graphics_QuadPushBorder(&parent->quadBuffer, 0, 0, upper.x,
                            upper.y, 2.0, vec4f(1.0));

    Graphics_QuadFlush(&parent->quadBuffer);
    if(textureId >= 0){
        upper.x *= 0.3;
        xoff = 25;
        OpenGLTexture texture = Graphics_GetTextureInfo(textureId);
        Float aspect = texture.width / texture.height;
            Float h = upper.x / aspect;
        Float dy = (upper.y - h) * 0.5;
        upper.y = h;
        upper += vec2ui(xoff, dy);
        Graphics_ImagePush(&parent->quadImageBuffer, vec2ui(xoff, dy), upper, textureId);
        Graphics_ImageFlush(gl, &parent->quadImageBuffer);
    }

    Graphics_ComputeTransformsForFontSize(font, a_fontSize, &model);

    Graphics_PrepareTextRendering(font, &gl->projection, &model);
    vec2f p = Graphics_ComputeCenteringStart(font, text.c_str(), text.size(), &geometry);
    if(textureId >= 0){
        p.x += (Float)xoff;
    }

    vec4i color = DefaultGetUIColor(UIScopeLine) * 1.3;
    int pGlyph = -1;
    Graphics_PushText(font, p.x, p.y, (char *)text.c_str(), text.size(), color, &pGlyph);
    Graphics_FlushText(font);

    // restore the font transforms for all other components
    font->fontMath = fMath;
    return 0;
}

int ImageTextureWidget::OnRender(WidgetRenderContext *wctx){
    WidgetWindow *parent = wctx->window;
    OpenGLState *gl = wctx->state;
    OpenGLFont *font = &parent->font;
    vec2f upper(geometry.upper.x - geometry.lower.x, geometry.upper.y - geometry.lower.y);
    upper *= font->fontMath.invReduceScale;
    Graphics_ImagePush(&parent->quadImageBuffer, vec2ui(0,0), upper, textureId);
    Graphics_ImageFlush(gl, &parent->quadImageBuffer);
    return 0;
}

PopupWindow::PopupWindow() : WidgetWindow(600, 400, "Popup!"){
    WidgetStyle s0, s1;
    int off = 0;
    OpenGLState *state = Graphics_GetGlobalContext();

    s0.background = vec4f(0.9);
    s1.background = vec4f(1.0);
    b0.SetStyle(s0);
    b1.SetStyle(s1);

    b0.SetText("B0");
    b1.SetText("B1");
    b0.SetTextureId(Graphics_FetchTextureFor(state, FILE_EXTENSION_CUDA, &off));

    Geometry geometry = WidgetWindow::GetGeometry();
    b0.PreferredGeometry(vec2f(0.3, .24), geometry);
    b1.PreferredGeometry(vec2f(0.7, .24), geometry);

    image.SetGeometry(geometry, vec2f(0.1, 0.3), vec2f(0.7, 0.9));
    image.SetTextureId(Graphics_FetchTextureFor(state, FILE_EXTENSION_CUDA, &off));

    WidgetWindow::PushWidget(&image);
    WidgetWindow::PushWidget(&b0);
    WidgetWindow::PushWidget(&b1);
}

