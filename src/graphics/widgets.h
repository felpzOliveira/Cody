/* date = January 1st 2022 19:59 */
#pragma once
#include <geometry.h>
#include <vector>
#include <string>
#include <functional>
#include <x11_display.h>
#include <keyboard.h>
#include <shaders.h>
#include <graphics.h>

#define WIDGET_TRANSITION_SCALE 0

struct OpenGLState;
class Widget;
class WidgetWindow;

/*
* Because I feel like creating several different pieces I'll abuse C++
* here instead of the C-like we have been doing on the rest of the code.
*/

/*
* Signal-Slot pattern for widgets. Currently this is very simple.
* It is implemented as a simple list of functions to call, if a slot
* should be called by different threads than this might be need more
* support. I'm however not wanting to implement threading here.
*/

template<typename ...Args>
class Slot{
    public:
    bool init;
    std::function<void(Args...)> fn;

    Slot() : init(false){}
    Slot(std::function<void(Args...)> callback){ set_fn(callback); }
    ~Slot() = default;

    void operator()(Args... vals){
        call_fn(vals...);
    }

    void call_fn(Args... vals){
        if(init){
            fn(vals...);
        }
    }

    void set_fn(std::function<void(Args...)> callback){
        init = true;
        fn = callback;
    }
};

template<typename ...Args>
class Signal{
    public:
    std::vector<Slot<Args...>> slots;

    Signal() = default;
    ~Signal() = default;

    void Connect(Slot<Args...> &slot){
        slots.push_back(slot);
    }

    void Disconnect(Slot<Args...> &slot){
        for(auto it = slots.begin(); it != slots.end(); ){
            if(*it == slot){
                it = slots.erase(it);
            }else{
                it++;
            }
        }
    }

    void DisconnectAll(){
        slots.clear();
    }

    void Emit(Args... vals){
        for(auto &fn : slots){
            fn(vals...);
        }
    }
};

// Wrapper for everything that might be required to render a widget
struct WidgetRenderContext{
    OpenGLState *state;
    Float dt;
    WidgetWindow *window;

    // filled in by the window handler
    vec4f windowBackground; // give background for hacky AA
};

struct WidgetTransition{
    int type;
    Float scale;
    Float interval;
    Float dt;
    vec2f velocity;
    vec2f startValues;
    vec2f endValues;
    bool running;
};

// Overall style for things we can generic render
struct WidgetStyle{
    vec4f background;
    bool valid;
};

/*
* The design will be very simple. Components that require a specific window,
* i.e.: popup windows, error windows, specific windows for opening files, etc...
* must be extended from WidgetWindow, this will automatically create a new window
* and handle keyboard/mouse events, they also hold a list of widgets that are
* referenced by the window and trigger rendering on them.
*
* Components that are part of windows are referenced as widget and are described
* by the Widget class. It represents something that is rendered within a window,
* i.e.: buttons, text, textinput, etc...
* Custom widgets can be created by extending the Widget class. Doing so automatically
* allow you to have stuff like style rendering and input event handling for you. It also
* removes some of the boilerplate code needed to link rendering/updating components.
*/
class WidgetWindow{
    public:
    WindowX11 *window; // the main window for the widgets
    BindingMap *mapping; // keyboard event handling
    vec2f resolution; // window resolution
    std::vector<Widget *> widgetList; // the list of widgets registered

    // utilities for detecting entered/leave events
    Widget *lastWidget;

    // these rendering components are instancied as references from global context
    // and don't actually hold any memory
    OpenGLBuffer quadBuffer; // shared quad buffer
    OpenGLImageQuadBuffer quadImageBuffer; // shared image buffer
    OpenGLFont font; // shared font

    /*
    * Basic constructor for WidgetWindow, receives the resolution in
    * width and height, and the window title in 'name'. Be aware that
    * the window created cannot be resizable.
    */
    WidgetWindow();
    WidgetWindow(int width, int height, const char *name);

    /*
    * Opens the window for this component. Note that this is meant to be
    * used for components that wish to not initialize the window during
    * declaration.
    */
    void Open(int width, int height, const char *name);

    /*
    * Get a geometry instance reference that describes this window.
    */
    Geometry GetGeometry();

    /*
    * Makes this window target for rendering.
    */
    void MakeContext();

    /*
    * Triggers framebuffer swap for displaying the contents of the window.
    */
    void Update();

    /*
    * Adds a new widget into the widget list of this window. Whenever the window
    * is destroyed, the widget is destroyed as well so you don't need to track the
    * widget pointer.
    */
    void PushWidget(Widget *widget){ widgetList.push_back(widget); }

    /*
    * Triggers the rendering of the window. This will automatically call
    * MakeContext and Update as well as the rendering of all required widgets,
    * it is the main routine you want to use for updating the window.
    * Returns 1 in case *anything* in this window is animating and therefore
    * requires the rendering loop to not sleep, 0 otherwise.
    */
    int DispatchRender(WidgetRenderContext *wctx);

    /*
    * Destructor.
    */
    virtual ~WidgetWindow();
};

class Widget{
    public:
    Geometry geometry; // widget's geometry as a reference to where to find it
    WidgetStyle style; // basic global styles
    Signal<> signalClicked; // on click signal to be emitted
    Signal<> signalEntered; // on mouse entered signal to be emitted
    Signal<> signalExited; // on mouse exited signal to be emitted
    WidgetTransition transition; // used for animation purposes

    // geometry used as reference for aspect computation, usually parent geometry
    Geometry refGeometry;

    /*
    * Basic constructors.
    */
    Widget();

    /*
    * Sets the global style of this widget.
    */
    void SetStyle(WidgetStyle stl){ style = stl; style.valid = true; }

    /*
    * Starts a animation (transition) in this widget. If a transition is already
    * in place than this call is ignored.
    */
    void Animate(const WidgetTransition &wtransition);

    /*
    * Interrupts whatever animation is currently running and force the
    * widget to go to its end as if it was finished. Use a composition
    * with this routine and 'Animate' if you need to overwrite the animation
    * immediately. Be aware that interrupting animations can make for weird
    * effects, use with care.
    */
    void InterruptAnimation();

    /*
    * Computest the widget geometry *after* applying the transition in place.
    * Calling this is the same as advancing the animation by 'dt'. This routine
    * calls OnAnimationUpdate to allow childs to update, returns the new geometry
    * which is also updated to the widget's global geometry.
    */
    Geometry AdvanceAnimation(Float dt);

    /*
    * Sets the geometry of this widget as a reference to another geometry
    * and aspects. i.e.: it computes the geometry as a fraction of another
    * geometry.
    */
    void SetGeometry(const Geometry &geo, vec2f aspectX, vec2f aspectY);

    /*
    * Main routine to render a widget.
    */
    int Render(WidgetRenderContext *wctx);

    /*
    * Register a new slot to be invoked whenever a click event happens.
    */
    void AddClickedSlot(Slot<> slot);

    /*
    * Destructor.
    */
    virtual ~Widget();

    /*
    * Base mouse routine that triggers signals only.
    */
    virtual void OnClick(){ signalClicked.Emit(); }
    virtual void OnExited(){ signalExited.Emit(); }
    virtual void OnEntered(){ signalEntered.Emit(); }

    /*
    * Utility calls for updating the widget' content. It gives the new geometry
    * the widget is going to take after the update. The current geometry is kept
    * during this call and can be used for deformations computations. It also
    * informs if after this call the animation sequence is done with the flag
    * willFinish, in which cases implementations should adjust their variables
    * as the animation will stop.
    */
    virtual void OnAnimationUpdate(Float dt, Geometry newGeometry, bool willFinish){}

    /*
    * Widgets must be implement a OnRender method that *actually* do rendering.
    */
    virtual int OnRender(WidgetRenderContext *wctx) = 0;

    /*
    * If the widget has a preferred geometry makes it take it around center.
    */
    virtual void PreferredGeometry(vec2f center, Geometry parentGeo){}
};

/* Basic implementation */

class ButtonWidget : public Widget{
    public:
    Float baseFontSize; // base font size for the system, copy for animation
    Float a_fontSize, a_vel; // animation purpose variables
    std::string text; // the text written in the button
    int textureId; // texture in case a icon is to also be displayed

    // utilities for preferred dimensions and animations
    static uint kWidth;
    static uint kHeight;
    static Float kScaleUp;
    static Float kScaleDown;
    static Float kScaleDuration;

    /*
    * Basic constructors.
    */
    ButtonWidget();
    ButtonWidget(std::string val);

    /*
    * Sets the label for this button.
    */
    void SetText(std::string val){ text = val; }

    /*
    * Sets the textureId for the button icon.
    */
    void SetTextureId(int id){ textureId = id; }

    /*
    * Update the font size for rendering during animation.
    */
    virtual void OnAnimationUpdate(Float dt, Geometry newGeometry, bool wFinish) override;

    /*
    * Renders the contents of the button.
    */
    virtual int OnRender(WidgetRenderContext *) override;

    /*
    * Slot implementation for the mouse events.
    */
    virtual void OnClick() override;
    virtual void OnEntered() override;
    virtual void OnExited() override;

    /*
    * Sets the button to take geometry around center with kWidth and kHeight.
    */
    virtual void PreferredGeometry(vec2f center, Geometry parentGeo) override;
};

/*
* TODO: Images being a widget mean we need to do one render call per image
* because the projection gets re-computed for each widget. If we don't have many
* images at the same time in the screen it shouldn't be a problem but if there are
* than we might want to re-think this thing.
*/
class ImageTextureWidget : public Widget{
    public:
    uint textureId;

    /*
    * Basic constructors.
    */
    ImageTextureWidget(){}
    ImageTextureWidget(uint texId){ SetTextureId(texId); }

    /*
    * Sets the texture id for the widget.
    */
    void SetTextureId(uint texId){ textureId = texId; }

    /*
    * Renders the contents of the image.
    */
    virtual int OnRender(WidgetRenderContext *) override;
};

class PopupWindow : public WidgetWindow{
    public:
    ButtonWidget b0, b1;
    ImageTextureWidget image;
    PopupWindow();
};
