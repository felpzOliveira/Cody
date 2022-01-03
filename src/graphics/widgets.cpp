#include <widgets.h>
#include <app.h>
#include <glad/glad.h>

#define MOUSE_CALLBACK_HANDLER(handler, x, y, priv) do{\
    WidgetWindow *ww = (WidgetWindow *)priv;\
    vec2ui mouse(x, ww->resolution.y - y);\
    for(Widget *w : ww->widgetList){\
        if(Geometry_IsPointInside(&w->geometry, mouse)){\
            w->handler();\
            break;\
        }\
    }\
}while(0)

void WidgetWindowOnClick(int x, int y, void *priv){
    MOUSE_CALLBACK_HANDLER(OnClick, x, y, priv);
}

void WidgetWindowOnSizeChange(int w, int h, void *priv){
    WidgetWindow *ww = (WidgetWindow *)priv;
    ww->resolution = vec2f(w, h);
    ww->OnSizeChange();
}

// TODO: maybe cache the keyboard mapping?
WidgetWindow::WidgetWindow(int width, int height, const char *name){
    window = CreateWindowX11(width, height, name);
    SetNoResizable(window);

    mapping = KeyboardCreateMapping(window);
    resolution = vec2f((Float)width, (Float)height);
    KeyboardSetActiveMapping(mapping);
    RegisterOnMouseClickCallback(window, WidgetWindowOnClick, this);
    RegisterOnSizeChangeCallback(window, WidgetWindowOnSizeChange, this);

    // TODO: Set mapping callbacks
    // TODO: Cache mapping?
    SwapIntervalX11(window, 0);
    // TODO: Register inputs
}

void WidgetWindow::PushWidget(Widget *widget){
    widgetList.push_back(widget);
}

void WidgetWindow::MakeContext(){
    if(window){
        MakeContextX11(window);
    }
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

Widget::Widget(){}
Widget::~Widget(){}
void Widget::SetStyle(WidgetStyle stl){ style = stl; }
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
}

void Widget::Render(WidgetRenderContext *wctx){
    // Rendering of a widget is simple, we paint background
    // and call specifics to render itself
    ActivateViewportAndProjection(wctx->state, &geometry);
    Float fcol[] = {style.background.x, style.background.y,
                    style.background.z, style.background.w};

    glClearBufferfv(GL_COLOR, 0, fcol);
    glClearBufferfv(GL_DEPTH, 0, kOnes);

    OnRender(wctx);

    glDisable(GL_SCISSOR_TEST);
}

ButtonWidget::ButtonWidget() : Widget(){}
ButtonWidget::ButtonWidget(std::string val) : Widget(){ SetText(val); }
ButtonWidget::~ButtonWidget(){ clicked.DisconnectAll(); }

void ButtonWidget::SetText(std::string val){ text = val; }
void ButtonWidget::AddOnClickFn(std::function<void(void)> fn){ AddClickedSlot(Slot<>(fn)); }
void ButtonWidget::AddClickedSlot(Slot<> slot){ clicked.Connect(slot); }
void ButtonWidget::OnClick(){
    printf("Clicked button with color %g %g %g\n", style.background.x,
           style.background.y, style.background.z);
    clicked.Emit();
}

void ButtonWidget::OnRender(WidgetRenderContext *wctx){
    // TODO: Render
    //printf("Called rendering for button %s\n", text.c_str());
}

PopupWidget::PopupWidget() : Widget(), WidgetWindow(600, 400, "Popup!"){
    b0 = new ButtonWidget("B0");
    b1 = new ButtonWidget("B1");

    WidgetStyle s0, s1;

    OnSizeChange();

    s0.background = vec4f(0.6, 0.6, 0.6, 1.0);
    s1.background = vec4f(1.0, 1.0, 1.0, 1.0);
    b0->SetStyle(s0);
    b1->SetStyle(s1);

    PushWidget(b0);
    PushWidget(b1);
}

void PopupWidget::OnSizeChange(){
    geometry.lower = vec2ui(0, 0);
    geometry.upper = vec2ui(resolution.x, resolution.y);
    geometry.extensionX = vec2f(0, 1);
    geometry.extensionY = vec2f(0, 1);
    b0->SetGeometry(geometry, vec2f(.0, .5), vec2f(.0, .5));
    b1->SetGeometry(geometry, vec2f(.5, 1.), vec2f(.5, 1.));
}

PopupWidget::~PopupWidget(){
    if(b0) delete b0;
    if(b1) delete b1;
}

void PopupWidget::OnRender(WidgetRenderContext *wctx){
    // TODO: Render
    //printf("Calling rendering for popup\n");
    MakeContext();
    b0->Render(wctx);
    b1->Render(wctx);
    Update();
}

