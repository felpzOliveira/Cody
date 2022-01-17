#include <dbgwidget.h>
#include <app.h>
#include <graphics.h>
#include <sstream>
#include <glad/glad.h>
#include <dbgapp.h>

#define kDelta 0.9

void DbgStateReport(DbgState state, void *priv){
    if(priv){
        DbgWidgetButtons *wd = (DbgWidgetButtons *)priv;
        wd->OnStateChange(state);
    }
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
vec4f DbgWidgetButtons::ComputeGeometryFor(uint n, Float scaleDown){
    Float yScale = 1.25 / scaleDown + Epsilon;
    Float x0 = (0.25 * (Float)n);
    Float x1 = (0.25 * (Float)(n+1));
    // start with a guess based on the size of the button
    Float dx = (x1 - x0) *(1.0 - kDelta) * 0.5;
    Float dy = (1.0 - kDelta) * 0.5;
    // apply clamping based on the previous equations
    dx = Clamp(dx, (x1-x0-0.25*scaleDown) * 0.5+Epsilon, (x1-x0)*0.5-Epsilon);
    dy = Clamp(dy, (1.0 - scaleDown) * 0.5+Epsilon, 0.5-Epsilon);
    // compute width
    Float w = (x1 - dx) - (x0 + dx);
    // offset considering buttons are centered in parent
    Float p0 = (1.0 - 4 * w) * 0.5;
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
    // (x0, y0, x1, y1)
    dy *= yScale;
    return vec4f(p0 + n * w, dy, p0 + (n + 1.0) * w, 1.0 - dy);
}

Geometry DbgWidgetButtons::GetGeometryFor(DbgWidgetButtons *wd, Geometry base, uint n){
    ButtonWidget *arr[4] = { &wd->runButton, &wd->pauseButton,
                             &wd->stepButton, &wd->continueButton };
    if(n < 4){
        return arr[n]->geometry;
    }
    return Geometry();
}

void DbgWidgetButtons::OnStateChange(DbgState state){
    std::cout << "Called" << std::endl;
}

void DbgWidgetButtons::SetGeometry(const Geometry &geo, vec2f aspectX, vec2f aspectY){
    vec4f p;
    // set the global geoemtry and buttons geometry
    Widget::SetGeometry(geo, aspectX, aspectY);
    // TODO: Manually?
    p = ComputeGeometryFor(0);
    runButton.SetGeometry(geometry, vec2f(p.x, p.z), vec2f(p.y, p.w));

    p = ComputeGeometryFor(1);
    pauseButton.SetGeometry(geometry, vec2f(p.x, p.z), vec2f(p.y, p.w));

    p = ComputeGeometryFor(2);
    stepButton.SetGeometry(geometry, vec2f(p.x, p.z), vec2f(p.y, p.w));

    p = ComputeGeometryFor(3);
    continueButton.SetGeometry(geometry, vec2f(p.x, p.z), vec2f(p.y, p.w));

    //runButton.SetGeometry(geometry, vec2f(0, .25f)*kDelta, extY);
    //pauseButton.SetGeometry(geometry, vec2f(.25f, .5f)*kDelta, extY);
    //stepButton.SetGeometry(geometry, vec2f(0.5f, .75f)*kDelta, extY);
    //continueButton.SetGeometry(geometry, vec2f(0.75f, 1.f)*kDelta, extY);

    // TODO: Theme?
    runButton.SetText("●");
    runButton.SetTextColor(ColorFromHexf(0xff87ffaf));
    runButton.SetBorder(ButtonWidget::kRightEdgeMask);

    pauseButton.SetText("●");
    pauseButton.SetTextColor(ColorFromHexf(0xffff87af));
    pauseButton.SetBorder(ButtonWidget::kRightEdgeMask | ButtonWidget::kLeftEdgeMask);

    stepButton.SetText("●");
    stepButton.SetTextColor(ColorFromHexf(0xff87afff));
    stepButton.SetBorder(ButtonWidget::kRightEdgeMask | ButtonWidget::kLeftEdgeMask);

    continueButton.SetText("●");
    continueButton.SetTextColor(ColorFromHexf(0xffffffaf));
    continueButton.SetBorder(ButtonWidget::kLeftEdgeMask);
    // TODO: Buttons texture

    lastButton = nullptr;
    staticButtons.n = 0;
    animatedButtons.n = 0;
    staticButtons.list[staticButtons.n++] = &runButton;
    staticButtons.list[staticButtons.n++] = &pauseButton;
    staticButtons.list[staticButtons.n++] = &stepButton;
    staticButtons.list[staticButtons.n++] = &continueButton;
}

int DbgWidgetButtons::OnRender(WidgetRenderContext *wctx, const Transform &){
    int animating = 0;
    //Float fcol[] = {1.0f, 1.0f, 1.0f, 1.0f};
    //glClearBufferfv(GL_COLOR, 0, fcol);
    for(uint i = 0; i < staticButtons.n; i++){
        animating |= staticButtons.list[i]->UnprojectedRender(wctx);
    }

    for(uint i = 0; i < animatedButtons.n; i++){
        animating |= animatedButtons.list[i]->UnprojectedRender(wctx);
    }
    return animating;
}

void DbgWidgetButtons::OnAnimationUpdate(Float dt, Geometry newGeometry, bool willFinish){
    runButton.OnAnimationUpdate(dt, GetGeometryFor(this, newGeometry, 0), willFinish);
    pauseButton.OnAnimationUpdate(dt, GetGeometryFor(this, newGeometry, 1), willFinish);
    stepButton.OnAnimationUpdate(dt, GetGeometryFor(this, newGeometry, 2), willFinish);
    continueButton.OnAnimationUpdate(dt, GetGeometryFor(this, newGeometry, 3), willFinish);
}

void DbgWidgetButtons::OnClick(){
    ButtonWidget *arr[4] = { &runButton, &pauseButton, &stepButton, &continueButton };
    for(uint i = 0; i < 4; i++){
        Geometry geo = GetGeometryFor(this, geometry, i);
        if(Geometry_IsPointInside(&geo, mouse)){
            arr[i]->SetMouseCoordintes(mouse);
            arr[i]->SetSelected(true);
            arr[i]->OnClick();
        }else{
            arr[i]->SetSelected(false);
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
    ButtonWidget *arr[4] = { &runButton, &pauseButton, &stepButton, &continueButton };
    ButtonWidget *curr = nullptr;
    bool changes = true;
    vec2f m(x, y);
    for(uint i = 0; i < 4; i++){
        Geometry geo = GetGeometryFor(this, geometry, i);
        if(Geometry_IsPointInside(&geo, m)){
            curr = arr[i];
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
        for(int i = 0; i < 4; i++){
            if(!InsideRenderOrder(&animatedButtons, arr[i])){
                PushRenderOrder(&staticButtons, arr[i]);
            }
        }
    }
}

void DbgWidgetButtons::OnResize(int width, int height){
    printf("Called Dbg::OnResize\n");
    Geometry geo;
    geo.lower = vec2ui(0);
    geo.upper = vec2ui(width, height);
    SetGeometry(geo, vec2f(0.85, 1.0), vec2f(0.95, 1.0));
}

void InitializeDbgButtons(WidgetWindow *window, DbgWidgetButtons *dbgBt){
    // TODO: these values
    dbgBt->SetGeometry(window->GetGeometry(), vec2f(0.85, 1.0), vec2f(0.95, 1.0));
    DbgApp_RegisterStateChangeCallback(DbgStateReport, dbgBt);
    window->PushWidget(dbgBt);
}
