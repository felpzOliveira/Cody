/* date = January 14th 2022 21:36 */
#pragma once
#include <widgets.h>
#include <dbgapp.h>

/*
* Implementation of the debugger buttons
*/

struct RenderOrder{
    ButtonWidget *list[4];
    uint n;
};

class DbgWidgetButtons : public Widget{
    public:
    ButtonWidget runButton;
    ButtonWidget pauseButton;
    ButtonWidget stepButton;
    ButtonWidget continueButton;
    ButtonWidget *lastButton;
    RenderOrder animatedButtons;
    RenderOrder staticButtons;

    DbgWidgetButtons() = default;
    ~DbgWidgetButtons() = default;

    /*
    * Utilities for ordering the rendering.
    */
    void ResetOrders(){
        animatedButtons.n = 0;
        staticButtons.n = 0;
    }

    void PushRenderOrder(RenderOrder *order, ButtonWidget *bt){
        order->list[order->n++] = bt;
    }

    bool InsideRenderOrder(RenderOrder *order, ButtonWidget *bt){
        for(uint i = 0; i < order->n; i++){
            if(bt == order->list[i]) return true;
        }
        return false;
    }

    /*
    * Called whenever the debugger changes states. No action is required it is only
    * a centralized way to coordinate states in rendering.
    */
    void OnStateChange(DbgState state);

    /*
    * Override geometry settings.
    */
    virtual void SetGeometry(const Geometry &geo, vec2f aspectX, vec2f aspectY) override;

    /*
    * Mouse overrides.
    */
    virtual void OnClick() override;
    virtual void OnMotion(int x, int y) override;
    virtual void OnExited() override;
    virtual void OnEntered() override;

    /*
    * Rendering overrides.
    */
    virtual int OnRender(WidgetRenderContext *wctx, const Transform &transform) override;
    virtual void OnAnimationUpdate(Float dt, Geometry newGeometry, bool willFinish) override;

    /*
    * Resize override.
    */
    virtual void OnResize(int width, int height) override;

    /*
    * Computes the geometry of one of the buttons given by its id 'n' and
    * the global base geometry 'base'.
    */
    static Geometry GetGeometryFor(DbgWidgetButtons *wd, Geometry base, uint n);
    static vec4f ComputeGeometryFor(uint n, Float scaleDown=ButtonWidget::kScaleDown);
};

/*
* Initializes the debugger buttons for a given window.
*/
void InitializeDbgButtons(WidgetWindow *window, DbgWidgetButtons *dbgBt);

