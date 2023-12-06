#include <widgets.h>
#include <app.h>
#include <glad/glad.h>
#include <resources.h>
#include <theme.h>
#include <gl3corefontstash.h>
#include <timing.h>
#include <buffers.h>
#include <iostream>

/* Button widget constants */
uint ButtonWidget::kWidth  = 92;
uint ButtonWidget::kHeight = 32;
Float ButtonWidget::kScaleUp = 1.1;
Float ButtonWidget::kScaleDown = 0.9090909090909091;
Float ButtonWidget::kScaleDuration = 0.1;
uint ButtonWidget::kLeftEdgeMask   = (1 << 0);
uint ButtonWidget::kRightEdgeMask  = (1 << 1);
uint ButtonWidget::kTopEdgeMask    = (1 << 2);
uint ButtonWidget::kBottonEdgeMask = (1 << 3);
uint ButtonWidget::kAllEdgesMask   = ButtonWidget::kLeftEdgeMask |
                                     ButtonWidget::kRightEdgeMask|
                                     ButtonWidget::kTopEdgeMask  |
                                     ButtonWidget::kBottonEdgeMask;

/* Text widget constants */
uint TextWidget::kAlignLeft   = (1 << 0);
uint TextWidget::kAlignCenter = (1 << 1);


void ActivateShaderAndProjections(Shader &shader, OpenGLState *state,
                                  Transform wTransform)
{
    Transform model = state->scale * wTransform;
    OpenGLCHK(glUseProgram(shader.id));
    Shader_UniformMatrix4(shader, "projection", &state->projection.m);
    Shader_UniformMatrix4(shader, "modelView", &model.m);
}

void WidgetWindowOnSizeChange(int width, int height, void *priv){
    WidgetWindow *ww = (WidgetWindow *)priv;
    ww->Resize(width, height);
}

void WidgetWindowOnClick(int x, int y, void *priv){
    WidgetWindow *ww = (WidgetWindow *)priv;
    Geometry wwGeo = ww->GetGeometry();
    vec2f mouse = vec2f(x, ww->window->height - y);
    if(ww->lastMotion){
        ww->lastMotion->isDragging = false;
    }

    if(!Geometry_IsPointInside(&wwGeo, mouse)){
        for(Widget *w : ww->widgetList) w->SetSelected(false);
        return;
    }

    ww->mouse = mouse;
    ww->lastClicked = nullptr;
    for(Widget *w : ww->widgetList){
        if(Geometry_IsPointInside(&w->geometry, ww->mouse)){
            w->SetMouseCoordintes(ww->mouse);
            w->SetSelected(true);
            w->OnClick();
            ww->lastClicked = w;
        }else{
            w->SetSelected(false);
        }
    }
}

void WidgetWindowOnPress(int x, int y, void *priv){
    /* since we already handle the on motion we don't actually need to compute widget */
    WidgetWindow *ww = (WidgetWindow *)priv;
    if(ww->lastMotion && ww->lastMotion->isDraggable){
        Geometry geo = ww->lastMotion->GetGeometry();
        vec2f dragRegionX = ww->lastMotion->dragRegionX;
        vec2f dragRegionY = ww->lastMotion->dragRegionY;
        y = ww->window->height - y;
        Float w = geo.upper.x - geo.lower.x;
        Float h = geo.upper.y - geo.lower.y;
        Float xs = ((Float)(x - geo.lower.x)) / w;
        Float ys = ((Float)(y - geo.lower.y)) / h;
        bool inside = ((xs >= dragRegionX.x && xs <= dragRegionX.y) &&
        (ys >= dragRegionY.x && ys <= dragRegionY.y));
        if(inside){
            ww->lastMotion->isDragging = true;
            ww->lastMotion->dragStart = vec2ui(x, y);
        }
    }
}

void WidgetWindowOnScroll(int is_up, void *priv){
    WidgetWindow *ww = (WidgetWindow *)priv;
    for(Widget *w : ww->widgetList){
        if(Geometry_IsPointInside(&w->geometry, ww->mouse)){
            w->OnScroll(is_up);
            break;
        }
    }
}

void WidgetWindowOnMotion(int x, int y, void *priv){
    // This works nicely but how is performance for doing this on motion?
    WidgetWindow *ww = (WidgetWindow *)priv;
    Geometry wwGeo = ww->GetGeometry();
    vec2f mouse = vec2f(x, ww->window->height - y);
    ww->mouse = mouse;
    if(ww->lastMotion && ww->lastMotion->isDragging){
        Geometry geo = ww->lastMotion->GetGeometry();
        Float w = geo.Width();
        Float h = geo.Height();
        Float mw = wwGeo.Width();
        Float mh = wwGeo.Height();
        Float ox = geo.lower.x;
        Float oy = geo.lower.y;
        Float dx = mouse.x - ww->lastMotion->dragStart.x;
        Float dy = mouse.y - ww->lastMotion->dragStart.y;

        Float nx = ox + dx, ux = nx + w;
        Float ny = oy + dy, uy = ny + h;
        if(((nx >= 0 && nx <= mw) && (ux >= 0 && ux <= mw) &&
        (ny >= 0 && ny <= mh) && (uy >= 0 && uy <= mh)) || true)
        {
            bool overlaps = false;
            geo.lower = vec2ui(nx, ny);
            geo.upper = geo.lower + vec2ui(w, h);
            for(Widget *w : ww->widgetList){
                if(w == ww->lastMotion) continue;

                if(w->GetGeometry().Overlaps(geo)){
                    overlaps = true;
                    break;
                }
            }

            if(!overlaps){
                ww->lastMotion->geometry = geo;
            }
        }

        ww->lastMotion->dragStart = vec2f(mouse.x, mouse.y);
        return;
    }else if(ww->lastMotion){
        ww->lastMotion->isDragging = false;
    }

    if(!Geometry_IsPointInside(&wwGeo, mouse)){
        if(ww->lastMotion){
            ww->lastMotion->OnExited();
        }
        ww->lastMotion = nullptr;
        return;
    }

    Widget *currWidget = nullptr;
    for(Widget *w : ww->widgetList){
        if(Geometry_IsPointInside(&w->geometry, mouse)){
            currWidget = w;
            break;
        }
    }

    if(currWidget == nullptr){
        // either we left a widget or we never entered one
        if(ww->lastMotion){
            ww->lastMotion->SetMouseCoordintes(mouse);
            ww->lastMotion->OnExited();
        }
    }else{
        // if we are inside the same widget than nothing happened
        if(currWidget == ww->lastMotion){
            // nothing
        }else if(ww->lastMotion){
            // if the widgets are different than they are really close
            // and we entered one and left the other
            ww->lastMotion->SetMouseCoordintes(mouse);
            ww->lastMotion->OnExited();
            currWidget->SetMouseCoordintes(mouse);
            currWidget->OnEntered();
            currWidget->isDragging = false;
        }else{
            // there is no last widget, we entered a new one
            currWidget->SetMouseCoordintes(mouse);
            currWidget->OnEntered();
            currWidget->isDragging = false;
        }
    }

    // update the last widget
    ww->lastMotion = currWidget;
    // we need to update the mouse otherwise we can't scroll
    ww->mouse = mouse;

    // forward for childs
    if(currWidget){
        currWidget->OnMotion(mouse.x, mouse.y);
    }
}

void WidgetWindowDefaultEntry(char *utf8Data, int utf8Size, void *priv){
    WidgetWindow *ww = (WidgetWindow *)priv;
    printf("Entry for %s %p\n", utf8Data, ww);
    if(ww->lastClicked){
        ww->lastClicked->OnDefaultEntry(utf8Data, utf8Size);
    }
}

/*********************************/
// WidgetWindow class
/*********************************/
WidgetWindow::WidgetWindow(){
    window = nullptr;
    mapping = nullptr;
    detached = true;
    start = vec2f(0);
    resolution = vec2f(0);
}
WidgetWindow::WidgetWindow(int width, int height, const char *name){
    window = nullptr;
    mapping = nullptr;
    detached = true;
    start = vec2f(0);
    resolution = vec2f((Float)width, (Float)height);
    Open(width, height, name, nullptr);
}

WidgetWindow::WidgetWindow(DisplayWindow *other, BindingMap *bmapping,
                           vec2f lower, vec2f upper)
{
    window = nullptr;
    mapping = bmapping;
    detached = false;
    start = lower;
    resolution = vec2f(upper.x - lower.x, upper.y - lower.y);
    Open(resolution.x, resolution.y, NULL, other);
}

void WidgetWindow::ResetGeometry(){
    for(Widget *w : widgetList){
        w->ResetGeometry();
    }
}

void WidgetWindow::Resize(int width, int height){
    Float ow = resolution.x, oh = resolution.y;
    resolution = vec2f(width, height);
    for(Widget *widget : widgetList){
        Geometry geo = widget->GetGeometry();
        Float x0 = (Float)geo.lower.x / ow;
        Float y0 = (Float)geo.lower.y / oh;

        widget->OnResize(width, height);

        if(widget->isDraggable){
            geo = widget->GetGeometry();
            Float w = geo.Width(), h = geo.Height();
            Float x = x0 * resolution.x;
            Float y = y0 * resolution.y;
            geo.lower = vec2ui(x, y);
            geo.upper = geo.lower + vec2ui(w, h);
            widget->geometry = geo;
        }
    }
}

void WidgetWindow::Open(int width, int height, const char *name, DisplayWindow *other){
    if(window){
        OpenGLBufferContextDelete(&quadBuffer);
        OpenGLBufferContextDelete(&quadImageBuffer);
        glfonsDeleteContext(font.fsContext);

        if(detached){
            DestroyWindow(window);
        }else{
            for(uint handle : eventHandles){
                UnregisterCallbackByHandle(window, handle);
            }
        }
    }

    window = other;

    if(detached && !window){
        //DisplayWindow *shared = (DisplayWindow *)Graphics_GetBaseWindow();
        // TODO: Add shared context for windows
        window = nullptr;
        //window = CreateWindowX11Shared(width, height, name, shared);
    }

    AssertA(window != nullptr, "Could not get window handle");

    MakeContext();
    if(detached){
        // TODO: Add resizable for windows
        //SetNotResizable(window);
        SwapInterval(window, 0);
        if(!mapping){
            mapping = KeyboardCreateMapping(window);
            RegisterKeyboardDefaultEntry(mapping, WidgetWindowDefaultEntry, this);
            KeyboardSetActiveMapping(mapping);
        }
    }

    // TODO: How to handle default entry when sharing 'other'?

    OpenGLState *state = Graphics_GetGlobalContext();
    OpenGLFont *ofont = &state->font;
    lastMotion = nullptr;
    lastClicked = nullptr;

    eventHandles.push_back(
        RegisterOnMouseLeftClickCallback(window, WidgetWindowOnClick, this)
    );
    eventHandles.push_back(
        RegisterOnMouseMotionCallback(window, WidgetWindowOnMotion, this)
    );
    eventHandles.push_back(
        RegisterOnScrollCallback(window, WidgetWindowOnScroll, this)
    );
    eventHandles.push_back(
        RegisterOnMousePressedCallback(window, WidgetWindowOnPress, this)
    );
    eventHandles.push_back(
        RegisterOnSizeChangeCallback(window, WidgetWindowOnSizeChange, this)
    );

    OpenGLFontCopy(&font, ofont);

    OpenGLBufferInitializeFrom(&quadBuffer, &state->glQuadBuffer);
    OpenGLImageBufferInitializeFrom(&quadImageBuffer, &state->glQuadImageBuffer);
    font.fsContext = glfonsCreateFrom(ofont->fsContext);
}

Geometry WidgetWindow::GetGeometry(){
    Geometry geo;
    geo.lower = vec2ui(start.x, start.y);
    geo.upper = vec2ui(start.x + resolution.x, start.y + resolution.y);
    return geo;
}

void WidgetWindow::MakeContext(){
    if(window){
        MakeContextCurrent(window);
    }
}

int WidgetWindow::DispatchRender(WidgetRenderContext *wctx){
    int is_animating = 0;
    if(detached) MakeContext();
    Geometry geometry = GetGeometry();
    vec4f background = DefaultGetUIColorf(UISelectorBackground);
    ActivateViewportAndProjection(wctx->state, &geometry);
    if(detached){
        Float fcol[] = {background.x, background.y, background.z, background.w};
        glClearBufferfv(GL_COLOR, 0, fcol);
        glClearBufferfv(GL_DEPTH, 0, kOnes);
    }

    // fill in window specifics
    wctx->windowBackground = background;
    wctx->window = this;

    for(Widget *w : widgetList){
        glEnable(GL_SCISSOR_TEST);
        is_animating |= w->Render(wctx);
    }

    glDisable(GL_SCISSOR_TEST);
    if(detached) Update();
    return is_animating;
}

void WidgetWindow::Update(){
    if(window){
        if(!WindowShouldClose(window)){
            SwapBuffers(window);
        }else{
            OpenGLBufferContextDelete(&quadBuffer);
            OpenGLBufferContextDelete(&quadImageBuffer);
            glfonsDeleteContext(font.fsContext);
            DestroyWindow(window);
            window = nullptr;
            Graphics_RequestClose(this);
        }
    }
}

WidgetWindow::~WidgetWindow(){
    if(window){
        OpenGLBufferContextDelete(&quadBuffer);
        OpenGLBufferContextDelete(&quadImageBuffer);
        glfonsDeleteContext(font.fsContext);
        if(detached){
            DestroyWindow(window);
        }
    }
}

/*********************************/
// Widget base class
/*********************************/
Widget::Widget(){
    transition.running = false;
    style.valid = false;
    selected = false;
    enabled = true;
    isDraggable = false;
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
        if(wtransition.type == WIDGET_TRANSITION_BACKGROUND && !style.valid){
            AssertA(0, "Cannot do background transition without valid style");
        }

        Float w0 = geometry.upper.x - geometry.lower.x;
        Float h0 = geometry.upper.y - geometry.lower.y;
        Float scale = Max(Epsilon, wtransition.scale);

        transition.scale = scale;
        transition.type = wtransition.type;
        transition.interval = Max(Epsilon, wtransition.interval);
        transition.dt = 0;
        transition.running = true;
        transition.is_out = wtransition.is_out;
        transition.cFloat.Begin(vec2f(w0 * scale, h0 * scale), transition.interval);
        transition.cFloat.Set(vec2f(w0, h0));
        transition.cFloat4.Begin(wtransition.background, transition.interval);
        transition.cFloat4.Set(style.background);
        Timing_Update();
    }
}

void Widget::SetGeometry(const Geometry &geo, vec2f aspectX, vec2f aspectY){
    Float w = geo.upper.x - geo.lower.x;
    Float h = geo.upper.y - geo.lower.y;
    uint x0 = geo.lower.x + (uint)(w * aspectX.x);
    uint x1 = geo.lower.x + (uint)(w * aspectX.y);
    uint y0 = geo.lower.y + (uint)(h * aspectY.x);
    uint y1 = geo.lower.y + (uint)(h * aspectY.y);

    geometry.lower = vec2ui(Min(x0, x1), Min(y0, y1));
    geometry.upper = vec2ui(Max(x0, x1), Max(y0, y1));
    geometry.extensionX = aspectX;
    geometry.extensionY = aspectY;
    center = geometry.Center();
    refGeometry = geo;
    baseLower = geometry.lower;
}

void Widget::OnResize(int width, int height){
    geometry.lower = center - vec2f(width, height) * 0.5;
    geometry.upper = center + vec2f(width, height) * 0.5;
}

Geometry Widget::AdvanceAnimation(Float dt){
    if(!transition.running) return geometry;
    Geometry geo = geometry;

    if(transition.type == WIDGET_TRANSITION_SCALE){
        vec2f res;
        Float x0 = 0, x1 = 0, y0 = 0, y1 = 0;

        transition.cFloat.Advance(dt, res);
        Float w02 = res.x * 0.5;
        Float h02 = res.y * 0.5;

        x0 = center.x - w02;
        x1 = center.x + w02;
        y0 = center.y - h02;
        y1 = center.y + h02;

        geo.lower = vec2ui(x0, y0);
        geo.upper = vec2ui(x1, y1);
    }else if(transition.type == WIDGET_TRANSITION_BACKGROUND){
        vec4f background;
        transition.cFloat4.Advance(dt, background);
        style.background = background;
    }
    else{
        // not yet supported or a child transition
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
    if(style.valid){
        //vec4f col = style.background;
        //Float fcol[] = {col.x, col.y, col.z, col.w};
        //glClearBufferfv(GL_COLOR, 0, fcol);
    }

    animating |= OnRender(wctx, wTransform);

    return animating;
}

int Widget::UnprojectedRender(WidgetRenderContext *wctx){
    int animating = transition.running ? 1 : 0;
    if(animating){
        geometry = AdvanceAnimation(wctx->dt);
    }

    ActivateViewportAndProjection(wctx->state, &geometry, false);
    if(style.valid){
        //vec4f col = style.background;
        //Float fcol[] = {col.x, col.y, col.z, col.w};
        //glClearBufferfv(GL_COLOR, 0, fcol);
    }

    animating |= OnRender(wctx, wTransform);

    return animating;
}

void Widget::AddClickedSlot(Slot<Widget *> slot){
    signalClicked.Connect(slot);
}

void Widget::DisconnectClickedSlots(){
    signalClicked.DisconnectAll();
}

/*********************************/
// Button widget
/*********************************/
ButtonWidget::ButtonWidget() : Widget(){
    baseFontSize = Graphics_GetFontSize();
    a_fontSize.Set((Float)baseFontSize);
    textureId = -1;
    buttonColor.w = -1;
    borderMask = 0;
    paint.valid = false;
    isScaledUp = false;
    transitionStyle.background = style.background;
    transitionStyle.obackground = style.background;
    transitionStyle.scaleUp = ButtonWidget::kScaleUp;
    transitionStyle.type = WIDGET_TRANSITION_SCALE;
}
ButtonWidget::ButtonWidget(std::string val) : Widget(){
    SetText(val);
    baseFontSize = Graphics_GetFontSize();
    a_fontSize.Set((Float)baseFontSize);
    textureId = -1;
    buttonColor.w = -1;
    borderMask = 0;
    transitionStyle.background = style.background;
    transitionStyle.obackground = style.background;
    transitionStyle.scaleUp = ButtonWidget::kScaleUp;
    transitionStyle.type = WIDGET_TRANSITION_SCALE;
}

void ButtonWidget::PreferredGeometry(vec2f point, Geometry parentGeo){
    Float w = parentGeo.upper.x - parentGeo.lower.x;
    Float h = parentGeo.upper.y - parentGeo.lower.y;
    Float ux = w * point.x, uy = h * point.y;
    uint x0 = parentGeo.lower.x + ux - ButtonWidget::kWidth / 2.0;
    uint x1 = parentGeo.lower.x + ux + ButtonWidget::kWidth / 2.0;
    uint y0 = parentGeo.lower.y + uy - ButtonWidget::kHeight / 2.0;
    uint y1 = parentGeo.lower.y + uy + ButtonWidget::kHeight / 2.0;
    geometry.lower = vec2ui(Min(x0, x1), Min(y0, y1));
    geometry.upper = vec2ui(Max(x0, x1), Max(y0, y1));
    geometry.extensionX = vec2f((Float)x0 / w, (Float)x1 / w);
    geometry.extensionY = vec2f((Float)y0 / h, (Float)y1 / h);
    center = geometry.Center();
    refGeometry = parentGeo;
    baseLower = geometry.lower;
}

void ButtonWidget::OnClick(){
    //printf("Clicked button with color %g %g %g\n", style.background.x,
           //style.background.y, style.background.z);
    Widget::OnClick();
}

void ButtonWidget::OnEntered(){
    if(!IsEnabled()) return;
    WidgetTransition transition;
        transition.type = transitionStyle.type;
        transition.scale = transitionStyle.scaleUp;
        transition.interval = ButtonWidget::kScaleDuration;
        transition.dt = 0;
        transition.running =  true;
        transition.background = transitionStyle.background;
        transition.is_out = false;

    Widget::OnEntered();
    Widget::InterruptAnimation();
    Widget::SetSelected(true);

    a_fontSize.Begin(((Float)baseFontSize) * transition.scale, transition.interval);
    a_fontSize.Set((Float)baseFontSize);
    isScaledUp = true;
    Widget::Animate(transition);
}

void ButtonWidget::OnExited(){
    if(!isScaledUp) return;
    Float invScale = 1.0 / transitionStyle.scaleUp;
    WidgetTransition transition;
        transition.type = transitionStyle.type;
        transition.scale = invScale;
        transition.interval = ButtonWidget::kScaleDuration;
        transition.dt = 0;
        transition.running =  true;
        transition.background = transitionStyle.obackground;
        transition.is_out = true;

    Widget::OnExited();
    Widget::InterruptAnimation();

    a_fontSize.Begin(((Float)baseFontSize) * transition.scale, transition.interval);
    a_fontSize.Set((Float)baseFontSize);
    isScaledUp = false;
    Widget::Animate(transition);
}

void ButtonWidget::OnAnimationUpdate(Float dt, Geometry newGeometry, bool willFinish){
    if(transition.type == WIDGET_TRANSITION_SCALE){
        if(willFinish){
            // we need to make sure the ending point for the font
            // is *exactly* the scaling term otherwise float point
            // precision could kill the view on chained animation
            Float eValue = 0;
            a_fontSize.Interrupt(eValue);
            baseFontSize = eValue;
        }else{
            a_fontSize.Advance(dt);
        }
    }else if(transition.type == WIDGET_TRANSITION_BACKGROUND){
        if(willFinish && transition.is_out){
            Widget::SetSelected(false);
        }
    }
}

int ButtonWidget::OnRender(WidgetRenderContext *wctx, const Transform &transform){
    OpenGLState *gl = wctx->state;
    WidgetWindow *parent = wctx->window;
    OpenGLFont *font = &parent->font;
    FontMath fMath = font->fontMath;
    Transform model;
    uint xoff = 0;
    vec2f p0, p1;
    bool has_text = text.size();

    Shader shader = gl->buttonShader;
    vec2f upper(geometry.upper.x - geometry.lower.x, geometry.upper.y - geometry.lower.y);
    upper *= font->fontMath.invReduceScale;

    ActivateShaderAndProjections(shader, gl);
    vec4f back = DefaultGetUIColorf(UISelectorBackground);
    if(style.valid){
        back = style.background;
    }

    p0 = transform.Point(vec2f(0));
    p1 = transform.Point(vec2f(upper));
    Graphics_QuadPush(&parent->quadBuffer, p0, p1, back);

    // TODO: thickness and color
    uint thick = 2;
    if(borderMask & kAllEdgesMask){
        p0 = transform.Point(vec2f(0, 0));
        p1 = transform.Point(vec2f(upper.x, upper.y));
        Graphics_QuadPushBorder(&parent->quadBuffer, p0.x, p0.y,
                                p1.x, p1.y, thick, vec4f(1.0));
    }else{
        if(borderMask & ButtonWidget::kLeftEdgeMask){
            p0 = transform.Point(vec2f(0, thick));
            p1 = transform.Point(vec2f(thick, upper.y-thick));
            Graphics_QuadPush(&parent->quadBuffer, p0, p1, vec4f(1.0));
        }

        if(borderMask & ButtonWidget::kRightEdgeMask){
            p0 = transform.Point(vec2f(upper.x-thick, thick));
            p1 = transform.Point(vec2f(upper.x, upper.y-thick));
            Graphics_QuadPush(&parent->quadBuffer, p0, p1, vec4f(1.0));
        }

        if(borderMask & ButtonWidget::kTopEdgeMask){
            p0 = transform.Point(vec2f(0, upper.y-thick));
            p1 = transform.Point(vec2f(upper.x, upper.y));
            Graphics_QuadPush(&parent->quadBuffer, p0, p1, vec4f(1.0));
        }

        if(borderMask & ButtonWidget::kBottonEdgeMask){
            p0 = transform.Point(vec2f(0, 0));
            p1 = transform.Point(vec2f(upper.x, thick));
            Graphics_QuadPush(&parent->quadBuffer, p0, p1, vec4f(1.0));
        }
    }

    Graphics_QuadFlush(&parent->quadBuffer);
    if(textureId >= 0){
        vec2f res = ImageTextureWidget::AspectPreservingResolution(upper, textureId);
        Float dy = (upper.y - res.y) * 0.5;
        Float dx = (upper.x - res.x) * 0.5;
        if(has_text){
            upper.x *= 0.3;
            xoff = 25;
            upper.y = res.y;
            upper += vec2ui(xoff, dy);
            p0 = transform.Point(vec2ui(xoff, dy));
        }else{
            p0 = transform.Point(vec2ui(dx, dy));
            upper = p0 + res;
        }
        p1 = transform.Point(upper);
        Graphics_ImagePush(&parent->quadImageBuffer, p0, p1, textureId);
        Graphics_ImageFlush(gl, &parent->quadImageBuffer);
    }

    if(has_text){
        Float fSize = 0;
        a_fontSize.Get(fSize);
        Graphics_ComputeTransformsForFontSize(font, fSize, &model);
        Graphics_PrepareTextRendering(font, &gl->projection, &model);
        vec2f p = Graphics_ComputeCenteringStart(font, text.c_str(),
                                                text.size(), &geometry, true);

        p = transform.Point(p + vec2f(xoff, 0));
        vec4i color(255);
        if(buttonColor.w > 0){
            color = ColorFromRGBA(buttonColor);
        }

        if(!IsEnabled()){
            color = vec4i(128);
        }
        int pGlyph = -1;
        Graphics_PushText(font, p.x, p.y, (char *)text.c_str(),
                          text.size(), color, &pGlyph);
        Graphics_FlushText(font);
    }

    // restore the font transforms for all other components
    font->fontMath = fMath;
    if(paint.valid){
        paint.paintFn(wctx, transform, geometry);
    }

    return 0;
}

/*********************************/
// Image widget
/*********************************/
vec2f ImageTextureWidget::AspectPreservingResolution(vec2f base, uint textureId){
    OpenGLTexture texture = Graphics_GetTextureInfo(textureId);
    Float aspect = texture.width / texture.height;
    Float w = base.x;
    Float h = base.x / aspect;
    Float err = h - base.y;
    if(h > base.y){
        w = base.y * aspect;
        h = base.y;
        if(w > base.x && err < (w-base.x)){
            w = base.x;
            h = base.x / aspect;
        }
    }

    return vec2f(w, h);
}

void ImageTextureWidget::PushImageIntoState(WidgetRenderContext *wctx, uint textureId,
                                            Geometry geometry, Transform eTransform)
{
    WidgetWindow *parent = wctx->window;
    OpenGLFont *font = &parent->font;
    vec2f upper(geometry.upper.x - geometry.lower.x, geometry.upper.y - geometry.lower.y);
    vec2f res = AspectPreservingResolution(upper, textureId);
    vec2f p = (upper - res) * 0.5;
    vec2f p0 = p * font->fontMath.invReduceScale;
    vec2f p1 = (p+res) * font->fontMath.invReduceScale;
    p0 = eTransform.Point(p0);
    p1 = eTransform.Point(p1);
    Graphics_ImagePush(&parent->quadImageBuffer, p0, p1, textureId);
}

int ImageTextureWidget::OnRender(WidgetRenderContext *wctx, const Transform &transform){
    WidgetWindow *parent = wctx->window;
    OpenGLState *gl = wctx->state;
    PushImageIntoState(wctx, textureId, geometry, transform);
    Graphics_ImageFlush(gl, &parent->quadImageBuffer);
    return 0;
}

/*********************************/
// Text widget
/*********************************/
int TextWidget::OnRender(WidgetRenderContext *wctx, const Transform &transform){
    OpenGLState *gl = wctx->state;
    WidgetWindow *parent = wctx->window;
    OpenGLFont *font = &parent->font;
    FontMath fMath = font->fontMath;
    Shader shader = gl->buttonShader;
    Float ypos = 0;
    Float px = ((Float)(geometry.upper.x - geometry.lower.x));
    Float py = ((Float)(geometry.upper.y - geometry.lower.y)) / lineOffset;
    if((Float)lines.size() > lineOffset){
        py = ((Float)(geometry.upper.y - geometry.lower.y)) / (Float)lines.size();
    }

    vec2f upper(geometry.upper.x - geometry.lower.x, geometry.upper.y - geometry.lower.y);
    upper *= font->fontMath.invReduceScale;

    ActivateShaderAndProjections(shader, gl, transform);

    Transform model;
    Graphics_ComputeTransformsForFontSize(font, fontSize, &model);
    model = model * transform;

    Graphics_PrepareTextRendering(font, &gl->projection, &model);
    for(std::string &str : lines){
        Geometry geo;
        int pGlyph = -1;
        geo.lower = vec2f(0, ypos);
        geo.upper = vec2f(px, ypos + py);
        if(str.size() > 0){
            uint size = str.size();
            char *text = (char *)str.c_str();
            vec2f p = Graphics_ComputeCenteringStart(font, text, size, &geo);
            if(alignment & TextWidget::kAlignLeft){
                p.x = 0;
            }
            Graphics_PushText(font, p.x, p.y, text, size, color, &pGlyph);
        }
        ypos += py;
    }

    Graphics_FlushText(font);
    font->fontMath = fMath;
    return 0;
}

/*********************************/
// Text table widget
/*********************************/

// Rendering this component is done in multiple passes to take advantage
// of batching primitives.
int TextTableWidget::OnRender(WidgetRenderContext *wctx, const Transform &transform){
    OpenGLState *gl = wctx->state;
    WidgetWindow *parent = wctx->window;
    OpenGLFont *font = &parent->font;
    FontMath fMath = font->fontMath;
    Float xpos = 0, ypos = 0;
    Transform model;
    Geometry focusGeo;
    vec2f pLoc;
    Float py = ((Float)(geometry.upper.y - geometry.lower.y)) / (Float)GetRowCount();

    Shader shader = gl->buttonShader;
    vec2f upper(geometry.upper.x - geometry.lower.x, geometry.upper.y - geometry.lower.y);
    upper *= font->fontMath.invReduceScale;

    ActivateShaderAndProjections(shader, gl, transform);

    // batch text first so we can compute cursor offset
    ypos = 0;
    for(uint s = 0; s < table.size(); s++){
        TextRow *row = &table[s];
        xpos = 0;

        for(uint i = 0; i < columns; i++){
            Geometry geo;
            int pGlyph = -1;
            int in_focus = s == focus.x && i == focus.y;
            std::string data = row->At(i);
            vec4i color = DefaultGetUIColor(UIScopeLine) * 1.3;
            Float px = ext[i] * ((Float)(geometry.upper.x - geometry.lower.x));
            if(in_focus && selected){
                color = vec4i(240, 30, 190, 255); // TODO: Theme?
            }

            geo.lower = vec2f(xpos, ypos);
            geo.upper = vec2f(xpos + px, ypos + py);
            if(data.size() > 0){
                uint size = data.size();
                char *text = (char *)data.c_str();
                vec2f p = Graphics_ComputeCenteringStart(font, text, size, &geo);
                if(in_focus) pLoc = p;
                Graphics_PushText(font, p.x, p.y, text, size, color, &pGlyph);
            }

            xpos += px;
        }

        ypos += py;
    }

    ypos = 0;
    xpos = 0;
    uint thickness = 1;
    Float thick2 = (Float)thickness * 0.5;
    // batch all quads
    Graphics_QuadPushBorder(&parent->quadBuffer, 0, 0, upper.x, upper.y, 2, vec4f(1.0));
    //Graphics_QuadPush(&parent->quadBuffer, vec2ui(0, upper.y-2), vec2ui(upper.x, upper.y), vec4f(1.0));
    for(uint s = 0; s < table.size(); s++){
        xpos = 0;
        for(uint i = 0; i < columns-1; i++){
            Geometry geo;
            Float px = ext[i] * ((Float)(geometry.upper.x - geometry.lower.x));
            geo.lower = vec2f(xpos, ypos);
            geo.upper = vec2f(xpos + px, ypos + py);

            //Float x0 = geo.lower.x * font->fontMath.invReduceScale;
            Float x0 = (geo.upper.x-thick2) * font->fontMath.invReduceScale;
            Float x1 = (geo.upper.x+thick2) * font->fontMath.invReduceScale;
            //Float y0 = geo.lower.y * font->fontMath.invReduceScale;
            Float y0 = (geo.lower.y) * font->fontMath.invReduceScale;
            Float y1 = (geo.upper.y) * font->fontMath.invReduceScale;

            vec4f col(1.0);

            Graphics_QuadPush(&parent->quadBuffer, vec2ui(x0, y0),
                              vec2ui(x1,y1), col);
            xpos += px;
            if(s == focus.x && i == focus.y){
                focusGeo = geo;
            }
        }

        if(s > 0){
            vec4f col(1.0);
            Float y0 = (ypos-thick2) * font->fontMath.invReduceScale;
            Float y1 = (ypos+thick2) * font->fontMath.invReduceScale;
            Graphics_QuadPush(&parent->quadBuffer, vec2ui(0, y0),
            vec2ui(upper.x, y1), col);
        }

        ypos += py;
    }

    if(selected){
        TextRow *focusedRow = &table[focus.x];
        uint cpos = focusedRow->Cursor(focus.y);
        cpos = 1;
        OpenGLCursor cursor;
        std::string val = focusedRow->At(focus.y);

        OpenGLComputeCursor(font, &cursor, (char *)val.c_str(),
                            val.size(), cpos, pLoc.x, pLoc.y);

        Float x0 = cursor.pMin.x, x1 = cursor.pMax.x;
        Float y0 = cursor.pMin.y, y1 = cursor.pMax.y;
        vec4f color = DefaultGetUIColorf(UICursor);
        Graphics_RenderCursorAt(&parent->quadBuffer, x0, y0, x1, y1, color, 3, 1);
    }

    Graphics_QuadFlush(&parent->quadBuffer);

    // render the text
    Graphics_ComputeTransformsForFontSize(font, 19, &model);
    model = model * transform;
    Graphics_PrepareTextRendering(font, &gl->projection, &model);
    Graphics_FlushText(font);
    font->fontMath = fMath;

    return 0;
}

void TextTableWidget::OnClick(){
    Float xpos = 0, ypos = 0;
    Float dy = (geometry.upper.y - geometry.lower.y);
    vec2f local = mouse - geometry.lower;
    local.y = dy - local.y; // reverse orientation for up->down events
    Float py = ((Float)dy) / (Float)GetRowCount();
    bool done = false;
    for(uint s = 0; s < table.size() && !done; s++){
        xpos = 0;
        for(uint i = 0; i < columns && !done; i++){
            Geometry geo;
            Float px = ext[i] * ((Float)(geometry.upper.x - geometry.lower.x));

            geo.lower = vec2f(xpos, ypos);
            geo.upper = vec2f(xpos + px, ypos + py);
            if(Geometry_IsPointInside(&geo, local)){
                focus = vec2ui(s, i);
                done = true;
                break;
            }
            xpos += px;
        }

        ypos += py;
    }
    // always call base for user signal handling
    Widget::OnClick();
}

void ScrollableWidget::OnScroll(int is_up){
    if(widget){
        Float xm = is_up ? motion.y + 10 : motion.y - 10;
        motion = vec2f(0, xm);
    }
}

int ScrollableWidget::OnRender(WidgetRenderContext *wctx, const Transform &){
    int animating = 0;
    if(widget){
        uint x0 = geometry.lower.x;
        uint y0 = geometry.lower.y;
        uint w = geometry.upper.x - geometry.lower.x;
        uint h = geometry.upper.y - geometry.lower.y;
        glScissor(x0, y0, w, h);
        glEnable(GL_SCISSOR_TEST);
        widget->SetWidgetTransform(Translate(motion.x, motion.y, 0));
        animating |= widget->UnprojectedRender(wctx);
    }
    return animating;
}

