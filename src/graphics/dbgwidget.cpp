#include <dbgwidget.h>
#include <app.h>
#include <graphics.h>
#include <sstream>
#include <glad/glad.h>
#include <dbgapp.h>
#include <queue>
#include <stack>

/**********************************************************************/
//                 Expressions implementation                         //
/**********************************************************************/
void DbgExpression(ExpressionFeedback feedback, void *priv){
    DbgWidgetExpressionViewer *viewer = (DbgWidgetExpressionViewer *)priv;
    viewer->SetExpression(&feedback);
}

template<typename Fn, typename ...Args>
void TransverseExpressionTreeNode(ExpressionTreeNode *root, int bfs, Fn fn, Args... args){
    struct NodeContext{
        ExpressionTreeNode *node;
        int depth;
        int count;
    };

    std::queue<NodeContext> nodeQueue;
    std::stack<NodeContext> nodeStack;

    if(bfs){
        nodeQueue.push({ .node = root, .depth = 0, .count = 0, });
    }else{
        nodeStack.push({ .node = root, .depth = 0, .count = 0, });
    }

    bool empty = false;
    while(!empty){
        NodeContext ctx;
        if(bfs){
            ctx = nodeQueue.front();
            nodeQueue.pop();
        }else{
            ctx = nodeStack.top();
            nodeStack.pop();
        }

        ExpressionTreeNode *node = ctx.node;
        int depth = ctx.depth;
        int count = ctx.count;
        int n = 0;

        if(fn(node, depth, count, args...)){
            for(ExpressionTreeNode *nd : node->nodes){
                if(bfs){
                    nodeQueue.push({ .node = nd, .depth = depth+1, .count = n, });
                }else{
                    nodeStack.push({ .node = nd, .depth = depth+1, .count = n, });
                }
                n++;
            }
        }

        empty = bfs ? nodeQueue.empty() : nodeStack.empty();
    }
}

template<typename Fn, typename ...Args>
void TransverseExpressionTreeActiveNodes(DbgWidgetExpressionViewer *dbgV,
                                         Fn fn, Args... args)
{
    ExpressionTreeNode *root = dbgV->expTree.root;
    auto call_fn = [&](ExpressionTreeNode *node, int depth, int count) -> int{
        fn(node, depth, count, args...);
        return (dbgV->states[node->identifier] == ExpViewState::Expanded ||
                dbgV->states[node->identifier] == ExpViewState::Static) ? 1 : 0;
    };

    TransverseExpressionTreeNode(root, false, call_fn);
}

Geometry GetNodeGeometryIterative(int depth, Float cellWidth,
                                  Float cellHeight, Float &y)
{
    Geometry geo;
    //Float d = 0.1 * cellWidth;
    Float d = 19;
    Float x = depth * d;
    geo.lower = vec2f(x, y);
    geo.upper = vec2f(x + cellWidth, y + cellHeight);
    y += cellHeight;
    return geo;
}

DbgWidgetExpressionViewer::DbgWidgetExpressionViewer(){
    state = ExpressionNone;
    expTree.root = nullptr;
    processClick = false;
}

DbgWidgetExpressionViewer::~DbgWidgetExpressionViewer(){
    if(expTree.root) Dbg_FreeExpressionTree(expTree);
}

void DbgWidgetExpressionViewer::Clear(){
    state = ExpressionNone;
    expression = std::string();
    if(expTree.root) Dbg_FreeExpressionTree(expTree);
    states.clear();
}

Geometry DbgWidgetExpressionViewer::ComputeRenderGeometry(WidgetRenderContext *wctx,
                                                          const Transform &transform)
{
    Float y0 = 0;
    Geometry res;
    bool has_data = false;
    auto fn = [&](ExpressionTreeNode *node, int depth, int count) -> int{
        Geometry geo = GetNodeGeometryIterative(depth, cellWidth, cellHeight, y0);
        geo.upper = transform.Point(geo.upper);
        geo.lower = transform.Point(geo.lower);
        if(has_data){
            res = geo;
            has_data = true;
        }else{
            res.lower = Min(Min(res.lower, geo.lower), geo.upper);
            res.upper = Max(Max(res.upper, geo.lower), geo.upper);
        }
        return 1;
    };

    TransverseExpressionTreeActiveNodes(this, fn);
    return res;
}

void DbgWidgetExpressionViewer::ProcessClickEvent(WidgetRenderContext *wctx,
                                                  const Transform &transform)
{
    Float y0 = 0;
    Geometry g = GetGeometry();
    vec2f m(mouse.x - g.lower.x, mouse.y - g.lower.y);
    m.y = g.Height() - m.y;

    Geometry rGeo = ComputeRenderGeometry(wctx, transform);
    if(!Geometry_IsPointInside(&rGeo, m)) return;

    WidgetWindow *parent = wctx->window;
    OpenGLFont *font = &parent->font;

    m *= font->fontMath.invReduceScale;
    auto fn = [&](ExpressionTreeNode *node, int depth, int count) -> int{
        Geometry geo = GetNodeGeometryIterative(depth, cellWidth, cellHeight, y0);
        vec2f lower = geo.lower * font->fontMath.invReduceScale;
        vec2f upper = geo.upper * font->fontMath.invReduceScale;
        geo.lower = transform.Point(lower);
        geo.upper = transform.Point(upper);

        if(Geometry_IsPointInside(&geo, m)){
            int id = node->identifier;
            if(states[id] == ExpViewState::Contracted){
                states[id] = ExpViewState::Expanded;
            }else if(states[id] == ExpViewState::Expanded){
                states[id] = ExpViewState::Contracted;
            }
        }
        return 1;
    };
    TransverseExpressionTreeActiveNodes(this, fn);
}

void DbgWidgetExpressionViewer::OnClick(){
    if(!expTree.root) return;
    processClick = true;
}

void DbgWidgetExpressionViewer::SetExpression(ExpressionFeedback *feedback){
    states.clear();
    auto fn = [&](ExpressionTreeNode *node, int depth, int count) -> int{
        if(node->nodes.size() > 0){
            states.push_back(ExpViewState::Contracted);
        }else{
            states.push_back(ExpViewState::Static);
        }
        node->identifier = states.size() - 1;

        return 1;
    };

    if(expTree.root) Dbg_FreeExpressionTree(expTree);
    if(feedback->rv){
        Dbg_ParseEvaluation((char *)feedback->value.c_str(), expTree);
        state = ExpressionValid;
        expTree.expression = feedback->expression;
        TransverseExpressionTreeNode(expTree.root, false, fn);
    }else{
        state = ExpressionInvalid;
    }

    expression = feedback->expression;
}

int DbgWidgetExpressionViewer::OnRender(WidgetRenderContext *wctx,
                                        const Transform &transform)
{
    vec4f col(0.2,0.2,0.2,1.0);
    Float fcol[] = {col.x, col.y, col.z, col.w};
    glClearBufferfv(GL_COLOR, 0, fcol);

    OpenGLState *gl = wctx->state;
    WidgetWindow *parent = wctx->window;
    OpenGLFont *font = &parent->font;
    Shader shader = gl->buttonShader;

    if(!expTree.root) return 0;
    if(processClick){
        ProcessClickEvent(wctx, transform);
        processClick = false;
    }

    Float y1 = 0;
    struct QuadInfo{
        vec2f p0, p1;
        int depth;
    };
    std::vector<QuadInfo> rects;

    auto fn_text = [&](ExpressionTreeNode *node, int depth, int count) -> int{
        Geometry geo = GetNodeGeometryIterative(depth, cellWidth, cellHeight, y1);
        int pGlyph = -1;
        std::string text;
        text = states[node->identifier] == ExpViewState::Contracted ? "[C]" : "[E]";
        text += depth == 0 ? expTree.expression : node->values.owner;
        text += " : ";
        text += node->values.value;
        vec4i color(255);
        vec2f lower = geo.lower * font->fontMath.invReduceScale;
        vec2f upper = geo.upper * font->fontMath.invReduceScale;
        vec2f p0 = transform.Point(lower);
        vec2f p1 = transform.Point(upper);

        rects.push_back({ .p0 = p0, .p1 = p1, .depth = depth });

        lower = (geo.lower + vec2f(0, 0.1 * cellHeight)) *
                            font->fontMath.invReduceScale;
        p0 = transform.Point(lower);

        Graphics_PushText(font, p0.x, p0.y, (char *)text.c_str(),
                          text.size(), color, &pGlyph);
        return 1;
    };

    Graphics_PrepareTextRendering(font, &gl->projection, &gl->model);
    TransverseExpressionTreeActiveNodes(this, fn_text);
    Graphics_FlushText(font);

    ActivateShaderAndProjections(shader, gl);

    for(QuadInfo &rect : rects){
        int depth = rect.depth;
        vec2f p0 = rect.p0;
        vec2f p1 = rect.p1;
        col = vec4f(1.0);
        if(depth == 1) col = vec4f(1.0, 0.0, 0.0, 1.0);
        if(depth == 2) col = vec4f(0.0, 1.0, 0.0, 1.0);
        if(depth == 3) col = vec4f(0.0, 0.0, 1.0, 1.0);
        if(depth == 4) col = vec4f(1.0, 0.0, 1.0, 1.0);
        if(depth == 5) col = vec4f(0.0, 1.0, 1.0, 1.0);
        Graphics_QuadPushBorder(&parent->quadBuffer, p0.x, p0.y, p1.x, p1.y, 2, col);
    }

    Graphics_QuadFlush(&parent->quadBuffer);

    return 0;
}

void DbgWidgetExpressionViewer::OnResize(int width, int height){
    Geometry geo;
    geo.lower = vec2ui(0);
    geo.upper = vec2ui(width, height);
    SetGeometry(geo, vec2f(0, .5), vec2f(0, .6));
}

void InitializeDbgExpressionViewer(WidgetWindow *window, DbgWidgetExpressionViewer *dbgVw){
    if(dbgVw->handle != 0){
        DbgApp_UnregisterCallbackByHandle(dbgVw->handle);
        dbgVw->handle = 0;
    }

    dbgVw->Clear();

    dbgVw->SetGeometry(window->GetGeometry(), vec2f(0, .5), vec2f(0, .6));
    dbgVw->SetDraggable(true);
    dbgVw->SetDragRegion(vec2f(0, 1), vec2f(0.9, 1));

    dbgVw->handle = DbgApp_RegisterExpressionFeedbackCallback(DbgExpression, dbgVw);
    if(!window->Contains(dbgVw)){
        window->PushWidget(dbgVw);
    }
}

/**********************************************************************/
//                     Buttons Implementation                         //
/**********************************************************************/
#define kDelta 0.9

void DbgStateReport(DbgState state, void *priv){
    if(priv){
        DbgWidgetButtons *wd = (DbgWidgetButtons *)priv;
        wd->OnStateChange(state);
    }
}

void DbgButtonOnStepClicked(Widget *){
    DbgApp_Step();
}

void DbgButtonOnNextClicked(Widget *){
    DbgApp_Next();
}

void DbgButtonOnFinishClicked(Widget *){
    DbgApp_Finish();
}

void DbgButtonOnMainButtonClicked(Widget *widget){
    ButtonWidget *bt = dynamic_cast<ButtonWidget*>(widget);
    DbgWidgetButtons *dbgBt = (DbgWidgetButtons *)bt->GetPrivate();
    if(dbgBt->buttonState == DbgButtonStateRun){
        DbgApp_Run();
    }else if(dbgBt->buttonState == DbgButtonStatePause){
        DbgApp_Interrupt();
    }else if(dbgBt->buttonState == DbgButtonStateContinue){
        DbgApp_Continue();
    }
}

void DbgButtonOnExitClicked(Widget *){
    DbgApp_Interrupt();
    DbgApp_Exit();
}

// We must consider the expansion animation of the button so we can correctly set its
// geometry otherwise the button might look weird during transitions.

// X-offset dx:
// w * scaleUp < 0.25 --- ((x1 - dx) - (x0 + dx)) * scaleUp < 0.25
// (x1-x0)-2dx < 0.25 * scaleDown --- 2dx > (x1-x0)-0.25*scaleDown
// dx > ((x1-x0)-0.25 * scaleDown)*0.5;
// w > 0 -- (x1-dx) - (x0+dx) > 0 -- x1-x0-2dx > 0 -- dx < (x1-x0)*0.5

// Y-offset dy:
// h * scaleUp < 1 --- (1.0 - 2dy) * scaleUp < 1
// 1.0 - 2dy < scaleDown --- 2dy > 1.0 - scaleDown
// dy > (1.0 - scaleDown) * 0.5
// h > 0 -- (1.0 - 2dy) > 0 -- dy > 0.5
vec4f DbgWidgetButtons::ComputeGeometryFor(uint n, Float scaleDown, uint count,
                                           int transitionType)
{
    if(transitionType == WIDGET_TRANSITION_SCALE){
        Float factor = 1.0 / (Float)Max(1, count);
        Float scaleRatio = (1.0 / scaleDown) / scaleDown + Epsilon;
        Float x0 = (factor * (Float)n);
        Float x1 = (factor * (Float)(n+1));
        // start with a guess based on the size of the button
        Float dx = (x1 - x0) *(1.0 - kDelta) * 0.5;
        Float dy = (1.0 - kDelta) * 0.5;
        // apply clamping based on the previous equations
        dx = Clamp(dx, (x1-x0-factor*scaleDown) * 0.5+Epsilon, (x1-x0)*0.5-Epsilon);
        dy = Clamp(dy, (1.0 - scaleDown) * 0.5+Epsilon, 0.5-Epsilon);
        // compute width
        Float w = (x1 - dx) - (x0 + dx);
        // offset considering buttons are centered in parent
        Float p0 = (1.0 - ((Float)count) * w) * 0.5;
        // one last consideration is required, the offset for dy.
        // if not done carefully the widgets animation might overlap
        // border renders and not show. We can add an ε term or slightly
        // scale the dy value, I'll scale it.
        //     ______________ 1.0
        //     --------------   1.0 - dy
        //        --------   1.0 - dy - ε
        //
        //        --------   dy + ε
        //     --------------   dy
        //     _______________ 0
        dy *= scaleRatio;
        // return value is (x0, y0, x1, y1) as proportional aspects
        return vec4f(p0 + n * w, dy, p0 + (n + 1.0) * w, 1.0 - dy);
    }else if(transitionType == WIDGET_TRANSITION_BACKGROUND){
        Float factor = 1.0 / (Float)Max(1, count);
        Float x0 = (factor * (Float)n);
        Float x1 = (factor * (Float)(n+1));
        Float w = x1 - x0;
        Float p0 = (1.0 - ((Float)count) * w) * 0.5;
        return vec4f(p0 + n * w, 0.0, p0 + (n + 1.0) * w, 1.0);
    }
    AssertA(0, "Unknown geometry computation\n");
    return vec4f();
}

Geometry DbgWidgetButtons::GetGeometryFor(DbgWidgetButtons *wd, Geometry base, uint n){
    if(n < DbgButtonCount){
        return wd->buttons[n].GetGeometry();
    }
    return Geometry();
}

void DbgWidgetButtons::SetMainButtonState(int state){
    buttonState = state;
    switch(state){
        case DbgButtonStateRun:{
            buttons[DbgRunIndex].SetText("●");
        } break;
        case DbgButtonStatePause:{
            buttons[DbgRunIndex].SetText(std::string());
        } break;
        case DbgButtonStateContinue:{
            buttons[DbgRunIndex].SetText("►");
        } break;
        default:{
            AssertA(0, "Unknown state");
        }
    }
}

void DbgWidgetButtons::OnStateChange(DbgState state){
    if(state == DbgState::Ready){
        GetButton(DbgStepIndex)->SetEnabled(false);
        GetButton(DbgFinishIndex)->SetEnabled(false);
        GetButton(DbgNextIndex)->SetEnabled(false);
        SetMainButtonState(DbgButtonStateRun);
    }else if(state == DbgState::Running){
        GetButton(DbgStepIndex)->SetEnabled(false);
        GetButton(DbgFinishIndex)->SetEnabled(false);
        GetButton(DbgNextIndex)->SetEnabled(false);
        SetMainButtonState(DbgButtonStatePause);
    }else if(state == DbgState::Signaled){
        GetButton(DbgStepIndex)->SetEnabled(true);
        GetButton(DbgFinishIndex)->SetEnabled(true);
        GetButton(DbgNextIndex)->SetEnabled(true);
        SetMainButtonState(DbgButtonStateContinue);
    }else if(state == DbgState::Break){
        GetButton(DbgStepIndex)->SetEnabled(true);
        GetButton(DbgFinishIndex)->SetEnabled(true);
        GetButton(DbgNextIndex)->SetEnabled(true);
        SetMainButtonState(DbgButtonStateContinue);
    }

    GetButton(DbgExitIndex)->SetEnabled(true);
    globalState = state;
}

void DbgWidgetButtons::SetGeometry(const Geometry &geo, vec2f aspectX, vec2f aspectY){
    vec4f p;
    // set the global geoemtry and buttons geometry
    Widget::SetGeometry(geo, aspectX, aspectY);
    // TODO: Manually?
    lastButton = nullptr;
    staticButtons.n = 0;
    animatedButtons.n = 0;
    vec4f colors[] = {
        ColorFromHexf(0xff87ffaf),
        ColorFromHexf(0xff87afff),
        ColorFromHexf(0xff87afff),
        ColorFromHexf(0xff87afff),
    };
    uint size = sizeof(colors) / sizeof(colors[0]);
    for(uint i = 0; i < DbgButtonCount; i++){
        p = ComputeGeometryFor(i, ButtonWidget::kScaleDown, DbgButtonCount,
                               buttons[i].transitionStyle.type);

        buttons[i].SetGeometry(geometry, vec2f(p.x, p.z), vec2f(p.y, p.w));
        switch(i){
            case DbgExitIndex:{
                buttons[DbgExitIndex].SetPaintFunction(
                    [&](WidgetRenderContext *wctx, const Transform &transform,
                        Geometry geo) -> void
                {
                    vec4f cc(1,0.6,0,1);
                    if(!buttons[DbgExitIndex].IsEnabled()) cc = vec4f(0.5);
                    uint thick = 2;
                    WidgetWindow *parent = wctx->window;
                    OpenGLFont *font = &parent->font;
                    vec2f upper(geo.upper.x - geo.lower.x, geo.upper.y - geo.lower.y);
                    upper *= font->fontMath.invReduceScale;

                    Float x0 = 0.375 * upper.x;
                    Float x1 = 0.625 * upper.x;
                    vec2f p0 = transform.Point(vec2f(x0, 0.3 * upper.y));
                    Float d = x1 - x0;
                    vec2f p1 = transform.Point(vec2f(x1, 0.3 * upper.y + d));
                    Graphics_QuadPushBorder(&parent->quadBuffer, p0.x, p0.y,
                                            p1.x, p1.y, thick, cc);
                    Graphics_QuadFlush(&parent->quadBuffer);
                });
            } break;
            case DbgStepIndex:{
                buttons[DbgStepIndex].SetText("▼");
            } break;
            case DbgNextIndex:{
                buttons[DbgNextIndex].SetPaintFunction(
                    [&](WidgetRenderContext *wctx, const Transform &transform,
                        Geometry geo) -> void
                {
                    vec4f cc = ColorFromHexf(0xff87afff);
                    if(!buttons[DbgNextIndex].IsEnabled()) cc = vec4f(0.5);
                    Transform model;
                    OpenGLState *gl = wctx->state;
                    WidgetWindow *parent = wctx->window;
                    OpenGLFont *font = &parent->font;
                    FontMath fMath = font->fontMath;
                    vec2f upper(geo.upper.x - geo.lower.x, geo.upper.y - geo.lower.y);
                    upper *= font->fontMath.invReduceScale;

                    // TODO: font size
                    Graphics_ComputeTransformsForFontSize(font, 19, &model);
                    Graphics_PrepareTextRendering(font, &gl->projection, &model);
                    int pGlyph = -1;
                    std::string text("-►");
                    vec2f p0 = Graphics_ComputeCenteringStart(font, text.c_str(),
                                                              text.size(), &geo, true);
                    p0 = transform.Point(p0);
                    Graphics_PushText(font, p0.x, p0.y, (char *)text.c_str(),
                                      text.size(), ColorFromRGBA(cc), &pGlyph);
                    Graphics_FlushText(font);
                    font->fontMath = fMath;
                });
            } break;
            case DbgFinishIndex:{
                buttons[DbgFinishIndex].SetText("▲");
            } break;
            case DbgRunIndex:{
                buttons[DbgRunIndex].SetText("●");
                buttons[DbgRunIndex].SetPaintFunction(
                    [&](WidgetRenderContext *wctx, const Transform &transform,
                        Geometry geo) -> void
                {
                    vec4f cc(0,0.6,1,1);
                    if(buttons[DbgRunIndex].text.size() > 0) return;
                    WidgetWindow *parent = wctx->window;
                    OpenGLFont *font = &parent->font;
                    vec2f upper(geo.upper.x - geo.lower.x, geo.upper.y - geo.lower.y);
                    upper *= font->fontMath.invReduceScale;

                    vec2f p0 = transform.Point(vec2f(0.4 * upper.x, 0.3 * upper.y));
                    vec2f p1 = transform.Point(vec2f(0.45 * upper.x, 0.6 * upper.y));
                    Graphics_QuadPush(&parent->quadBuffer, p0, p1, cc);

                    p0 = transform.Point(vec2f(0.55 * upper.x, 0.3 * upper.y));
                    p1 = transform.Point(vec2f(0.60 * upper.x, 0.6 * upper.y));
                    Graphics_QuadPush(&parent->quadBuffer, p0, p1, cc);
                    Graphics_QuadFlush(&parent->quadBuffer);
                });
            } break;
            default:{
                buttons[i].SetText("●");
                //AssertA(0, "Don't know how to render");
            }
        }

        //buttons[i].SetBorder(ButtonWidget::kAllEdgesMask);
        vec4f col = colors[i < size ? i : size-1];
        buttons[i].SetTextColor(col);

        staticButtons.list[staticButtons.n++] = &buttons[i];
    }
    OnStateChange(globalState);
}

int DbgWidgetButtons::OnRender(WidgetRenderContext *wctx, const Transform &){
    int animating = 0;
    vec4f col(0.2,0.2,0.2,1.0);
    Float fcol[] = {col.x, col.y, col.z, col.w};
    glClearBufferfv(GL_COLOR, 0, fcol);
    for(uint i = 0; i < staticButtons.n; i++){
        animating |= staticButtons.list[i]->UnprojectedRender(wctx);
    }

    for(uint i = 0; i < animatedButtons.n; i++){
        animating |= animatedButtons.list[i]->UnprojectedRender(wctx);
    }
    return animating;
}

void DbgWidgetButtons::OnAnimationUpdate(Float dt, Geometry newGeometry, bool willFinish){
    for(uint i = 0; i < DbgButtonCount; i++){
        buttons[i].OnAnimationUpdate(dt, GetGeometryFor(this, newGeometry, i), willFinish);
    }
}

void DbgWidgetButtons::OnClick(){
    for(uint i = 0; i < DbgButtonCount; i++){
        Geometry geo = GetGeometryFor(this, geometry, i);
        if(Geometry_IsPointInside(&geo, mouse)){
            buttons[i].SetMouseCoordintes(mouse);
            buttons[i].SetSelected(true);
            buttons[i].OnClick();
        }else{
            buttons[i].SetSelected(false);
        }
    }
}

void DbgWidgetButtons::OnExited(){
    if(lastButton) lastButton->OnExited();
    lastButton = nullptr;
}

void DbgWidgetButtons::OnEntered(){
    OnMotion(mouse.x, mouse.y);
}

void DbgWidgetButtons::OnMotion(int x, int y){
    ButtonWidget *curr = nullptr;
    bool changes = true;
    vec2f m(x, y);
    for(uint i = 0; i < DbgButtonCount; i++){
        Geometry geo = GetGeometryFor(this, geometry, i);
        if(Geometry_IsPointInside(&geo, m)){
            curr = &buttons[i];
            break;
        }
    }

    if(curr == nullptr){
        if(lastButton){
            lastButton->SetMouseCoordintes(m);
            lastButton->OnExited();
            ResetOrders();
            PushRenderOrder(&animatedButtons, lastButton);
        }else{
            changes = false;
        }
    }else{
        if(curr == lastButton){
            changes = false;
        }else if(lastButton){
            lastButton->SetMouseCoordintes(m);
            lastButton->OnExited();
            curr->SetMouseCoordintes(m);
            curr->OnEntered();
            ResetOrders();
            PushRenderOrder(&animatedButtons, lastButton);
            PushRenderOrder(&animatedButtons, curr);
        }else{
            curr->SetMouseCoordintes(m);
            curr->OnEntered();
            ResetOrders();
            PushRenderOrder(&animatedButtons, curr);
        }
    }

    lastButton = curr;
    mouse = m;
    if(changes){
        for(int i = 0; i < DbgButtonCount; i++){
            if(!InsideRenderOrder(&animatedButtons, &buttons[i])){
                PushRenderOrder(&staticButtons, &buttons[i]);
            }
        }
    }
}

void DbgWidgetButtons::OnResize(int width, int height){
    Geometry geo;
    geo.lower = vec2ui(0);
    geo.upper = vec2ui(width, height);
    SetGeometry(geo, vec2f(0.85, 1.0), vec2f(0.95, 1.0));
}

void InitializeDbgButtons(WidgetWindow *window, DbgWidgetButtons *dbgBt){
    // TODO: these values do not adjust when changing theme
    vec4f back(0.2,0.2,0.2,1.0);
    vec4f oback = 1.2 * back;
    if(CurrentThemeIsLight()){
        oback = back;
        back = 1.2 * oback;
    }
    TransitionStyle tStyle = {
        .background = oback,
        .obackground = back,
        .scaleUp = 1.0,
        .type = WIDGET_TRANSITION_BACKGROUND,
    };

    WidgetStyle style = {
        .background = back,
        .valid = true,
    };

    Float aspectX = ((Float)DbgButtonCount) * 0.1 / 4.0;
    Float endX = 0.95;
    for(int i = 0; i < DbgButtonCount; i++){
        dbgBt->buttons[i].SetPrivate(dbgBt);
        dbgBt->buttons[i].DisconnectClickedSlots();
        dbgBt->buttons[i].SetStyle(style);
        dbgBt->buttons[i].SetAnimateStyle(tStyle);
    }
    dbgBt->SetGeometry(window->GetGeometry(), vec2f(endX-aspectX, 1.0), vec2f(endX, 1.0));

    if(dbgBt->handle != 0){
        DbgApp_UnregisterCallbackByHandle(dbgBt->handle);
        dbgBt->handle = 0;
    }

    dbgBt->globalState = DbgState::Ready;
    dbgBt->buttons[DbgRunIndex].SetEnabled(true); // run button starts valid
    dbgBt->buttons[DbgRunIndex].AddClickedSlot(Slot<Widget *>(DbgButtonOnMainButtonClicked));

    dbgBt->buttons[DbgExitIndex].SetEnabled(true); // exit button starts valid
    dbgBt->buttons[DbgExitIndex].AddClickedSlot(Slot<Widget *>(DbgButtonOnExitClicked));

    dbgBt->buttons[DbgStepIndex].SetEnabled(false); // step button starts invalid
    dbgBt->buttons[DbgStepIndex].AddClickedSlot(Slot<Widget *>(DbgButtonOnStepClicked));

    dbgBt->buttons[DbgNextIndex].SetEnabled(false); // next button starts invalid
    dbgBt->buttons[DbgNextIndex].AddClickedSlot(Slot<Widget *>(DbgButtonOnNextClicked));

    dbgBt->buttons[DbgFinishIndex].SetEnabled(false); // finish button starts invalid
    dbgBt->buttons[DbgFinishIndex].AddClickedSlot(Slot<Widget *>(DbgButtonOnFinishClicked));

    dbgBt->handle = DbgApp_RegisterStateChangeCallback(DbgStateReport, dbgBt);
    if(!window->Contains(dbgBt)){
        window->PushWidget(dbgBt);
    }
    dbgBt->SetMainButtonState(DbgButtonStateRun);
}
