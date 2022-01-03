/* date = January 1st 2022 19:59 */
#pragma once
#include <geometry.h>
#include <vector>
#include <string>
#include <functional>
#include <x11_display.h>
#include <keyboard.h>

struct OpenGLState;

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

struct WidgetRenderContext{
    OpenGLState *state;
};

struct WidgetStyle{
    vec4f background;
};

class Widget;
class WidgetWindow{
    public:
    WindowX11 *window;
    BindingMap *mapping;
    vec2f resolution;
    std::vector<Widget *> widgetList;

    WidgetWindow(int width, int height, const char *name);
    void MakeContext();
    void Update();
    void PushWidget(Widget *widget);
    ~WidgetWindow();

    virtual void OnSizeChange() = 0;
};

class Widget{
    public:
    Geometry geometry;
    WidgetStyle style;

    Widget();
    void SetStyle(WidgetStyle stl);
    void SetGeometry(const Geometry &geo, vec2f aspectX, vec2f aspectY);
    void Render(WidgetRenderContext *);
    virtual ~Widget();
    virtual void OnRender(WidgetRenderContext *) = 0;
    virtual void OnClick(){}
};

class ButtonWidget : public Widget{
    public:
    Signal<> clicked;
    std::string text;
    ButtonWidget();
    ButtonWidget(std::string val);
    void SetText(std::string val);
    void AddClickedSlot(Slot<> slot);
    void AddOnClickFn(std::function<void(void)> fn);
    virtual ~ButtonWidget() override;
    virtual void OnRender(WidgetRenderContext *) override;
    virtual void OnClick() override;
};

class PopupWidget : public Widget, public WidgetWindow{
    public:
    ButtonWidget *b0, *b1;
    PopupWidget();
    virtual ~PopupWidget() override;
    virtual void OnRender(WidgetRenderContext *) override;
    virtual void OnSizeChange() override;
};
