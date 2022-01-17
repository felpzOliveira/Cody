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
#include <vector>
#include <sstream>

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
    vec2f start; // lower point of the rendering window
    std::vector<Widget *> widgetList; // the list of widgets registered
    vec2f mouse; // last user mouse event
    bool detached; // flag indicating if the window is detached from the main window
    std::vector<uint> eventHandles; // handles from the window
    bool is_transp;

    // utilities for detecting entered/leave events
    // as well as send typing events to a unique widget
    Widget *lastMotion, *lastClicked;

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
    * Returns the number of registered widgets in this window.
    */
    uint WidgetCount(){ return widgetList.size(); }

    /*
    * Construct the widgetwindow as a wrapper inside another window.
    * Input entries (i.e.: typing) must be carried out outside. We do however
    * handle mouse events.
    */
    WidgetWindow(WindowX11 *other, BindingMap *bmapping, vec2f lower, vec2f upper);

    /*
    * Triggers a resize of the widget window and all of its widgets.
    * Calling this does not change the lower point of the window geometry
    * only its extension.
    */
    void Resize(int width, int height);

    /*
    * Removes a widget from the widget list being rendered in this window.
    */
    void RemoveWidget(Widget *widget){
        for(auto it = widgetList.begin(); it != widgetList.end(); it++){
            if(*it == widget){
                widgetList.erase(it);
                break;
            }
        }
        if(widget == lastMotion) lastMotion = nullptr;
        if(widget == lastClicked) lastClicked = nullptr;
    }

    /*
    * Get the window handler for the widget.
    */
    WindowX11 *GetWindowHandler(){ return window; }

    /*
    * Opens the window for this component. Note that this is meant to be
    * used for components that wish to not initialize the window during
    * declaration.
    */
    void Open(int width, int height, const char *name, WindowX11 *other);

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

/*
* This is the base class all renderable widgets must extend. It provides some defaults
* for rendering, animating and handling mouse/keyboard events.
*/
class Widget{
    public:
    Geometry geometry; // widget's geometry as a reference to where to find it
    WidgetStyle style; // basic global styles
    Signal<> signalClicked; // on click signal to be emitted
    Signal<> signalEntered; // on mouse entered signal to be emitted
    Signal<> signalExited; // on mouse exited signal to be emitted
    Signal<int> signalScrolled; // on mouse scroll
    WidgetTransition transition; // used for animation purposes
    vec2f mouse; // last user click event address
    bool selected; // check if the user clicked on the widget, marked by main window
    Transform wTransform; // apply external transform to widget
    vec2f center; // widget center, cached for better animation

    // geometry used as reference for aspect computation, usually parent geometry
    Geometry refGeometry;

    /*
    * Basic constructors.
    */
    Widget();

    /*
    * Sets the selected value for the widget.
    */
    virtual void SetSelected(bool val){ selected = val; }

    /*
    * Sets the mouse coordinates inside the widget.
    */
    void SetMouseCoordintes(vec2ui coords){ mouse = coords; }

    /*
    * Sets the external widget transform. This must be consider in child,
    * the global rendering dispatch does not apply this transform. It is basically
    * a storage for childs to interact.
    */
    void SetWidgetTransform(const Transform &transform){ wTransform = transform; }

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
    virtual void SetGeometry(const Geometry &geo, vec2f aspectX, vec2f aspectY);

    /*
    * Main routine to render a widget. This routine adjusts globals for the widget
    * i.e.: style stuff. Sets the viewport for rendering and calls 'OnRender'.
    */
    int Render(WidgetRenderContext *wctx);

    /*
    * Same as 'Render' but does not update projections and viewport. Can be use
    * to chain rendering routines without touching globals.
    */
    int UnprojectedRender(WidgetRenderContext *wctx);

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
    virtual void OnScroll(int is_up){ signalScrolled.Emit(is_up); }
    virtual void OnMotion(int x, int y){}

    /*
    * Base keyboard routines. These routines depend on the widget being active
    * in the window, i.e.: the widget must be the last one the user clicked on.
    */
    virtual void OnDefaultEntry(char *utf8Data, uint utf8Size){}

    /*
    * Utility calls for updating the widget's content. It gives the new geometry
    * the widget is going to take after the update. The current geometry is kept
    * during this call and can be used for deformations computations. It also
    * informs if after this call the animation sequence is done with the flag
    * willFinish, in which cases implementations should adjust their variables
    * as the animation will stop.
    */
    virtual void OnAnimationUpdate(Float dt, Geometry newGeometry, bool willFinish){}

    /*
    * Widgets must implement the OnRender method that *actually* do rendering.
    * The rendering routine receives a transform that should be used as a Model
    * for rendering the components. This value is given by 'wTransform' and is
    * given here *again* as a stronger indicator that it should be applied to
    * all components either as a global shader matrix or a direct transform on
    * the coordinates being rendered, i.e.: *do not ignore it*. For base primitives
    * usually applying Transform::Point(...) is enough to perform the transformation.
    */
    virtual int OnRender(WidgetRenderContext *wctx, const Transform &transform) = 0;

    /*
    * If the widget has a preferred geometry makes it take it around center.
    * Some widgets like button can have a size that does not depend on the
    * parent window, calling this routine instead of 'SetGeometry' should
    * adjust thge widgets geometry to have that size but within the geometry
    * specified by 'parentGeo' and 'center'.
    */
    virtual void PreferredGeometry(vec2f center, Geometry parentGeo){
        AssertA(0, "Called geometry setting for unsupported widget");
    }

    /*
    * Widgets can implement the OnResize method that is able to resize the widget.
    * This means the parent has change geometry and the widget should also change.
    * The lower point of parents do not change so you may assume the widget as changed
    * size because of simple scale around its center point. Default implementation
    * simply changes the base geometry of the component but if your widget require more
    * than that... you should re-implement it.
    */
    virtual void OnResize(int width, int height);
};

/* Basic implementation */

class ButtonWidget : public Widget{
    public:
    Float baseFontSize; // base font size for the system, copy for animation
    Float a_fontSize, a_vel; // animation purpose variables
    std::string text; // the text written in the button
    int textureId; // texture in case a icon is to also be displayed
    vec4f buttonColor; // the color of the text on the button
    uint borderMask; // mask for rendering border segments

    // utilities for preferred dimensions and animations
    static uint kWidth;
    static uint kHeight;
    static Float kScaleUp;
    static Float kScaleDown;
    static Float kScaleDuration;

    static uint kLeftEdgeMask;
    static uint kRightEdgeMask;
    static uint kTopEdgeMask;
    static uint kBottonEdgeMask;
    static uint kAllEdgesMask;

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
    * Sets the border mask for the rendering of button.
    */
    void SetBorder(uint mask){ borderMask = mask; }

    /*
    * Sets the color of the text on the button.
    */
    void SetTextColor(vec4f col){ buttonColor = col; }

    /*
    * Update the font size for rendering during animation.
    */
    virtual void OnAnimationUpdate(Float dt, Geometry newGeometry, bool wFinish) override;

    /*
    * Renders the contents of the button.
    */
    virtual int OnRender(WidgetRenderContext *, const Transform &transform) override;

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
    ImageTextureWidget() : Widget(){}
    ImageTextureWidget(uint texId) : Widget(){ SetTextureId(texId); }

    /*
    * Sets the texture id for the widget.
    */
    void SetTextureId(uint texId){ textureId = texId; }

    /*
    * Renders the contents of the image.
    */
    virtual int OnRender(WidgetRenderContext *, const Transform &transform) override;

    /*
    * Compute a resolution to render a given texture 'textureId' that preserves
    * the aspect ratio of the texture that can be used to render inside a quad
    * of size 'base'.
    */
    static vec2f AspectPreservingResolution(vec2f base, uint textureId);

    /*
    * Computes and pushes a image texture given by 'textureId' into the accumulation
    * image buffer present in the state 'wctx' considering the size especified in
    * 'geometry'. This routine basically prepare the texture for rendering but does
    * not trigger the render routine with 'Graphics_ImageFlush'. Only caches it.
    */
    static void PushImageIntoState(WidgetRenderContext *wctx, uint textureId,
                                   Geometry geometry, Transform eTransform);
};

class TextRow{
    public:
    std::vector<std::string> data;
    std::vector<int> cursor;

    TextRow(uint cols){
        uint count = Max(1, cols);
        for(uint i = 0; i < count; i++){
            data.push_back(std::string());
            cursor.push_back(0);
        }
    }

    void Set(std::string str, uint at){
        if(at < data.size()){
            data[at] = str;
        }
    }

    std::string At(uint at){
        if(at < data.size()){
            return data[at];
        }

        return nullptr;
    }

    uint Cursor(uint entry){
        uint res = 0;
        if(entry < cursor.size()){
            res = cursor[entry];
        }
        return res;
    }

    ~TextRow() = default;
};

class TextTableWidget : public Widget{
    public:
    uint columns;
    std::vector<TextRow> table;
    std::vector<Float> ext;
    vec2ui focus;

    /*
    * Basic constructors.
    */
    TextTableWidget(){}
    TextTableWidget(uint cols){ SetColumns(cols); }

    /*
    * Set the columns var. Use for static allocations.
    */
    void SetColumns(uint cols){ columns = Max(1, cols); }

    /*
    * Adds empty new rows into the table.
    */
    void AddRows(uint n=1){
        for(uint i = 0; i < n; i++){
            table.push_back(TextRow(columns));
        }
    }

    /*
    * Set the value at a cell.
    */
    void SetItem(std::string str, uint i, uint j){
        if(table.size() > i && columns > j){
            table[i].Set(str, j);
        }
    }

    /*
    * Resize the table erasing all of its contents.
    */
    void Resize(uint rows, uint cols){
        columns = cols;
        table = std::vector<TextRow>();
        AddRows(rows);
    }

    /*
    * Get the number of rows in the table.
    */
    uint GetRowCount(){
        return table.size();
    }

    /*
    * Returns the current dimensions of the table.
    */
    vec2ui Dimensions(){
        return vec2ui(table.size(), columns);
    }

    /*
    * Sets the geometry of the table, applying custom size for each column.
    * The length of the extension vector must match the number of columns in
    * the table and must sum to 1 otherwise rendering will be undefined.
    */
    void SetGeometry(const Geometry &geo, vec2f aspectX, vec2f aspectY,
                     std::vector<Float> extensions)
    {
        if(columns != extensions.size()) return;
        // call base class to set global geometry
        Widget::SetGeometry(geo, aspectX, aspectY);

        ext = extensions;
    }

    /*
    * Sets the geometry of the table with uniform sizes for each column.
    */
    virtual void SetGeometry(const Geometry &geo, vec2f aspectX, vec2f aspectY) override{
        // call base class to set global geometry
        Widget::SetGeometry(geo, aspectX, aspectY);

        Float f = 1.0 / (Float)columns;
        for(uint i = 0; i < columns; i++){
            ext.push_back(f);
        }
    }

    /*
    * Renders the contents of the table.
    */
    virtual int OnRender(WidgetRenderContext *wctx, const Transform &transform) override;

    /*
    * Mouse overrides. Mostly for setting focus.
    */
    virtual void OnClick() override;

    ~TextTableWidget() = default;
};


/*
* I'm going to attempt to implement generic widget scrolling through
* model transformation. This means the underlying widget should always
* render itself to the fullest and we scroll by transforming with
* respect to the global widget transform. It's not really clean but it
* seems to work.
*/
class ScrollableWidget final : public Widget{
    public:
    Widget *widget;
    vec2f motion;

    /*
    * Basic constructor.
    */
    ScrollableWidget() : Widget(), widget(nullptr){}
    ScrollableWidget(Widget *targetWidget) : Widget(){ SetWidget(targetWidget); }

    ~ScrollableWidget() = default;

    /*
    * Sets the child widget.
    */
    void SetWidget(Widget *w){ widget = w; }

    /*
    * Override all mouse events because we are going to need to further them ourselves.
    * Because we only futher the event we don't expect signal bindings for the area itself
    * but only for the child widget. Trivial handlers.
    */
    virtual void OnClick() override{
        if(widget){
            widget->SetMouseCoordintes(mouse + motion);
            widget->SetSelected(true);
            widget->OnClick();
        }
    }

    virtual void OnEntered() override{
        if(widget){
            widget->OnEntered();
        }
    }

    virtual void OnExited() override{
        if(widget){
            widget->OnExited();
        }
    }

    virtual void SetSelected(bool val) override{
        if(widget){
            widget->SetSelected(val);
        }

        Widget::SetSelected(val);
    }

    /*
    * Special scroll override so we can compute the transform for the scrollable area.
    */
    virtual void OnScroll(int is_up) override;

    /*
    * Override the rendering routine.
    */
    virtual int OnRender(WidgetRenderContext *wctx, const Transform &transform) override;

    /*
    * Geometry resize override.
    */
    virtual void OnResize(int width, int height) override{
        if(widget){
            widget->OnResize(width, height);
        }
        Widget::OnResize(width, height);
    }

};

/* Test */
class PopupWindow : public WidgetWindow{
    public:
    ButtonWidget b0, b1, tbutton;
    ImageTextureWidget image, timage;
    TextTableWidget tt;
    ScrollableWidget sc;
    PopupWindow();
};
