#include <widgets.h>
#include <app.h>
#include <glad/glad.h>
#include <resources.h>
#include <theme.h>
#include <gl3corefontstash.h>
#include <timing.h>

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

// TODO: maybe cache the keyboard mapping?
WidgetWindow::WidgetWindow(int width, int height, const char *name){
    OpenGLState *state = Graphics_GetGlobalContext();
    WindowX11 *shared = (WindowX11 *)Graphics_GetBaseWindow();
    OpenGLFont *ofont = &state->font;
    window = CreateWindowX11Shared(width, height, name, shared);
    lastWidget = nullptr;
    SetNotResizable(window);

    mapping = KeyboardCreateMapping(window);
    resolution = vec2f((Float)width, (Float)height);
    KeyboardSetActiveMapping(mapping);
    RegisterOnMouseClickCallback(window, WidgetWindowOnClick, this);
    RegisterOnMouseMotionCallback(window, WidgetWindowOnMotion, this);

    // TODO: Set mapping callbacks
    // TODO: Cache mapping?
    SwapIntervalX11(window, 0);
    // TODO: Register inputs

    MakeContext();
    OpenGLFontCopy(&font, ofont);
    OpenGLBufferInitializeFrom(&quadBuffer, &state->glQuadBuffer);
    font.fsContext = glfonsCreateFrom(ofont->fsContext);
    if(font.fsContext == nullptr){
        printf("[Widget] Failed to create font locally\n");
    }
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
    Float fcol[] = {background.x, background.y,
                    background.z, background.w};

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
    for(Widget *w : widgetList){
        if(w){
            delete w;
        }
    }

    if(window){
        DestroyWindowX11(window);
    }
}

Widget::Widget(WidgetWindow *window){
    transition.running = false;
}
Widget::~Widget(){
    signalClicked.DisconnectAll();
}

void Widget::InterruptAnimation(){
    // simply simulate the maximum step
    geometry = GetAnimatedGeometry(transition.interval + Epsilon);
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

Geometry Widget::GetAnimatedGeometry(Float dt){
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
    transition.dt += dt;
    geometry = geo;
    transition.running = transition.dt <= transition.interval;
    if(!transition.running){
        printf("Finish at : ");
        geometry.PrintSelf();
    }

    return geo;
}

int Widget::Render(WidgetRenderContext *wctx){
    // Rendering of a widget is simple, we paint background
    // and call specifics to render itself
    int animating = transition.running ? 1 : 0;
    if(animating){
        geometry = GetAnimatedGeometry(wctx->dt);
    }
    ActivateViewportAndProjection(wctx->state, &geometry);
    Float fcol[] = {style.background.x, style.background.y,
                    style.background.z, style.background.w};

    glClearBufferfv(GL_COLOR, 0, fcol);

    animating |= OnRender(wctx);

    glDisable(GL_SCISSOR_TEST);
    return animating;
}

void Widget::AddClickedSlot(Slot<> slot){
    signalClicked.Connect(slot);
}

ButtonWidget::ButtonWidget(WidgetWindow *window) : Widget(window){}
ButtonWidget::ButtonWidget(WidgetWindow *window, std::string val) : Widget(window){
    SetText(val);
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

    Widget::OnExited();
    Widget::InterruptAnimation();
    Widget::Animate(transition);
}

int ButtonWidget::OnRender(WidgetRenderContext *wctx){
    OpenGLState *gl = wctx->state;
    WidgetWindow *parent = wctx->window;
    OpenGLFont *font = &parent->font;

    Shader shader = gl->buttonShader;
    vec2f upper(geometry.upper.x - geometry.lower.x, geometry.upper.y - geometry.lower.y);
    upper *= font->fontMath.invReduceScale;

    ActivateShaderAndProjections(shader, gl);
    Shader_UniformVec4(shader, "backgroundColor", wctx->windowBackground);
    Shader_UniformVec2(shader, "buttonResolution", upper);

    Graphics_QuadPush(&parent->quadBuffer, vec2ui(0,0), upper, style.background);
    Graphics_QuadFlush(&parent->quadBuffer);

    Graphics_PrepareTextRendering(font, &gl->projection, &gl->scale);
    vec2f p = Graphics_ComputeCenteringStart(font, text.c_str(), text.size(), &geometry);
    vec4i color = DefaultGetUIColor(UIScopeLine) * 1.3;
    int pGlyph = -1;
    Graphics_PushText(font, p.x, p.y, (char *)text.c_str(), text.size(), color, &pGlyph);
    Graphics_FlushText(font);

    return 0;
}

PopupWindow::PopupWindow() : WidgetWindow(600, 400, "Popup!"){
    ButtonWidget *b0 = new ButtonWidget(this, "B0");
    ButtonWidget *b1 = new ButtonWidget(this, "B1");

    WidgetStyle s0, s1;

    s0.background = vec4f(0.6, 0.6, 0.6, 1.0);
    s1.background = vec4f(1.0, 1.0, 1.0, 1.0);
    b0->SetStyle(s0);
    b1->SetStyle(s1);

    Geometry geometry = WidgetWindow::GetGeometry();
    b0->PreferredGeometry(vec2f(0.3, .24), geometry);
    b1->PreferredGeometry(vec2f(0.7, .24), geometry);

    WidgetWindow::PushWidget(b0);
    WidgetWindow::PushWidget(b1);
}

