/* date = January 14th 2022 21:36 */
#pragma once
#include <widgets.h>
#include <dbgapp.h>

/*
* Implementation of the debugger buttons
*/

#define DbgRunIndex      (0)
#define DbgStepIndex     (DbgRunIndex+1)
#define DbgFinishIndex   (DbgStepIndex+1)
#define DbgNextIndex     (DbgFinishIndex+1)
#define DbgExitIndex     (DbgNextIndex+1)

#define DbgMaxIndex (DbgExitIndex)
#define DbgButtonCount (DbgMaxIndex+1)

#define DbgButtonStateNone      -1
#define DbgButtonStateRun        0
#define DbgButtonStatePause      1
#define DbgButtonStateContinue   2

template<unsigned int N>
struct RenderOrder{
    ButtonWidget *list[N];
    uint n;
};

class DbgWidgetButtons : public Widget{
    public:
    ButtonWidget buttons[DbgButtonCount];
    bool states[DbgButtonCount];
    ButtonWidget *lastButton;
    RenderOrder<DbgButtonCount> animatedButtons;
    RenderOrder<DbgButtonCount> staticButtons;
    uint handle = 0;
    int buttonState = DbgButtonStateNone;
    DbgState globalState;

    DbgWidgetButtons() = default;
    ~DbgWidgetButtons() = default;

    /*
    * Utilities for ordering the rendering.
    */
    void ResetOrders(){
        animatedButtons.n = 0;
        staticButtons.n = 0;
    }

    /*
    * Add a button into a specific render order list.
    */
    void PushRenderOrder(RenderOrder<DbgButtonCount> *order, ButtonWidget *bt){
        order->list[order->n++] = bt;
    }

    /*
    * Checks if a button is inside a specific render order list.
    */
    bool InsideRenderOrder(RenderOrder<DbgButtonCount> *order, ButtonWidget *bt){
        for(uint i = 0; i < order->n; i++){
            if(bt == order->list[i]) return true;
        }
        return false;
    }

    /*
    * Get the handle to a specific button.
    */
    ButtonWidget *GetButton(uint index){
        index = Clamp(index, 0, DbgMaxIndex);
        return &buttons[index];
    }

    /*
    * Called whenever the debugger changes states. No action is required it is only
    * a centralized way to coordinate states in rendering.
    */
    void OnStateChange(DbgState state);

    /*
    * Sets the state of the pause button.
    */
    void SetMainButtonState(int state);

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
    static vec4f ComputeGeometryFor(uint n, Float scaleDown=ButtonWidget::kScaleDown,
                                    uint count=4, int transitionType=WIDGET_TRANSITION_SCALE);
};

/*
* Initializes the debugger buttons for a given window.
*/
void InitializeDbgButtons(WidgetWindow *window, DbgWidgetButtons *dbgBt);

