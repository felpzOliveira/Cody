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

static void ActivateShaderAndProjections(Shader &shader, OpenGLState *state,
                                         Transform wTransform = Transform())
{
    Transform model = state->scale * wTransform;
    OpenGLCHK(glUseProgram(shader.id));
    Shader_UniformMatrix4(shader, "projection", &state->projection.m);
    Shader_UniformMatrix4(shader, "modelView", &model.m);
}

void WidgetWindowOnClick(int x, int y, void *priv){
    WidgetWindow *ww = (WidgetWindow *)priv;
    ww->mouse = vec2f(x, ww->resolution.y - y);
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
        if(ww->lastMotion){
            ww->lastMotion->OnExited();
        }
    }else{
        // if we are inside the same widget than nothing happened
        if(currWidget == ww->lastMotion){
            // nothing
        }else if(ww->lastMotion){
            // if the widgets are different than they are really close
            // and we entered one and left the other
            ww->lastMotion->OnExited();
            currWidget->OnEntered();
        }else{
            // there is no last widget, we entered a new one
            currWidget->OnEntered();
        }
    }

    // update the last widget
    ww->lastMotion = currWidget;
    // we need to update the mouse otherwise we can't scroll
    ww->mouse = mouse;
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
    lastMotion = nullptr;
    lastClicked = nullptr;
    SetNotResizable(window);
    SwapIntervalX11(window, 0);

    if(!mapping){
        MakeContext();
        mapping = KeyboardCreateMapping(window);
        RegisterKeyboardDefaultEntry(mapping, WidgetWindowDefaultEntry, this);
        KeyboardSetActiveMapping(mapping);

        RegisterOnMouseLeftClickCallback(window, WidgetWindowOnClick, this);
        RegisterOnMouseMotionCallback(window, WidgetWindowOnMotion, this);
        RegisterOnScrollCallback(window, WidgetWindowOnScroll, this);

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
        glEnable(GL_SCISSOR_TEST);
        is_animating |= w->Render(wctx);
    }

    glDisable(GL_SCISSOR_TEST);
    Update();
    return is_animating;
}

void WidgetWindow::Update(){
    if(window){
        if(!WindowShouldCloseX11(window)){
            SwapBuffersX11(window);
        }else{
            OpenGLBufferContextDelete(&quadBuffer);
            OpenGLBufferContextDelete(&quadImageBuffer);
            glfonsDeleteContext(font.fsContext);
            DestroyWindowX11(window);
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
        DestroyWindowX11(window);
    }

    printf("Called destructor\n");
}

/*********************************/
// Widget base class
/*********************************/
Widget::Widget(){
    transition.running = false;
    style.valid = false;
    selected = false;
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
    if(!transition.running) return geometry;
    Geometry geo = geometry;

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
        vec4f col = style.background;
        Float fcol[] = {col.x, col.y, col.z, col.w};
        glClearBufferfv(GL_COLOR, 0, fcol);
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
        vec4f col = style.background;
        Float fcol[] = {col.x, col.y, col.z, col.w};
        glClearBufferfv(GL_COLOR, 0, fcol);
    }

    animating |= OnRender(wctx, wTransform);

    return animating;
}

void Widget::AddClickedSlot(Slot<> slot){
    signalClicked.Connect(slot);
}

/*********************************/
// Button widget
/*********************************/
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

int ButtonWidget::OnRender(WidgetRenderContext *wctx, const Transform &transform){
    OpenGLState *gl = wctx->state;
    WidgetWindow *parent = wctx->window;
    OpenGLFont *font = &parent->font;
    FontMath fMath = font->fontMath;
    Transform model;
    uint xoff = 0;
    vec2f p0, p1;

    Shader shader = gl->buttonShader;
    vec2f upper(geometry.upper.x - geometry.lower.x, geometry.upper.y - geometry.lower.y);
    upper *= font->fontMath.invReduceScale;

    ActivateShaderAndProjections(shader, gl);
    vec4f back = DefaultGetUIColorf(UISelectorBackground);
    Float scale = 1.0;
    if(!CurrentThemeIsLight()){
        scale = 1.2;
        back = Clamp(back * 1.4, vec4f(0), vec4f(1));
    }

    p0 = transform.Point(vec2f(0));
    p1 = transform.Point(vec2f(upper));
    Graphics_QuadPush(&parent->quadBuffer, p0, p1, back * scale);

    Graphics_QuadFlush(&parent->quadBuffer);
    if(textureId >= 0){
        upper.x *= 0.3;
        xoff = 25;
        vec2f res = ImageTextureWidget::AspectPreservingResolution(upper, textureId);
        Float dy = (upper.y - res.y) * 0.5;
        upper.y = res.y;
        upper += vec2ui(xoff, dy);
        p0 = transform.Point(vec2ui(xoff, dy));
        p1 = transform.Point(upper);
        Graphics_ImagePush(&parent->quadImageBuffer, p0, p1, textureId);
        Graphics_ImageFlush(gl, &parent->quadImageBuffer);
    }

    Graphics_ComputeTransformsForFontSize(font, a_fontSize, &model);
    Graphics_PrepareTextRendering(font, &gl->projection, &model);
    vec2f p = Graphics_ComputeCenteringStart(font, text.c_str(),
                                             text.size(), &geometry, true);

    p = transform.Point(p);
    vec4i color(255);
    int pGlyph = -1;
    Graphics_PushText(font, p.x, p.y, (char *)text.c_str(), text.size(), color, &pGlyph);
    Graphics_FlushText(font);

    // restore the font transforms for all other components
    font->fontMath = fMath;
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

PopupWindow::PopupWindow() : WidgetWindow(800, 400, "Popup!"){
    WidgetStyle s0;
    int off = 0;
    OpenGLState *state = Graphics_GetGlobalContext();
    uint tid0 = 0, tid1 = 0;

    tid0 = Graphics_FetchTextureFor(state, FILE_EXTENSION_CUDA, &off);
    tid1 = Graphics_FetchTextureFor(state, FILE_EXTENSION_LIT, &off);

    b0.SetText("B0");
    b1.SetText("B1");
    b0.SetTextureId(tid0);

    Geometry geometry = WidgetWindow::GetGeometry();
    b0.PreferredGeometry(vec2f(0.3, .24), geometry);
    b1.PreferredGeometry(vec2f(0.7, .24), geometry);

    image.SetGeometry(geometry, vec2f(0.0, 0.4), vec2f(0.5, 0.9));
    image.SetTextureId(tid1);

    timage.SetGeometry(geometry, vec2f(0.3, 0.95), vec2f(0.5, 0.9));
    timage.SetTextureId(tid1);

    tbutton.SetText("SCButton!");
    tbutton.SetGeometry(geometry, vec2f(0.3, 0.95), vec2f(0.5, 0.9));
    tbutton.SetTextureId(tid0);

    int nrows = 3;
    int ncols = 4;
    tt.SetColumns(ncols);
    tt.SetGeometry(geometry, vec2f(0.3, 0.95), vec2f(0.4, 0.95));
    tt.AddRows(nrows);

    for(int i = 0; i < nrows; i++){
        for(int j = 0; j < ncols; j++){
            std::stringstream ss;
            ss << "Item_" << i << "_" << j;
            tt.SetItem(ss.str(), i, j);
        }
    }

    sc.SetWidget(&tt);
    sc.SetGeometry(geometry, vec2f(0.3, 0.95), vec2f(0.5, 0.9));

    WidgetWindow::PushWidget(&image);
    WidgetWindow::PushWidget(&b0);
    WidgetWindow::PushWidget(&b1);
    WidgetWindow::PushWidget(&sc);
}

