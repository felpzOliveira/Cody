#include <viewer_ctrl.h>
#include <keyboard.h>
#include <app.h>
#include <geometry.h>

#define SoftDx 0.05f

#define ViewerSanityCheckRet(__var)\
    View *view = AppGetActiveView();\
    BufferView *bView = View_GetBufferView(view);\
    if(View_GetState(view) != View_ImageDisplay || BufferView_GetViewType(bView) != ImageView){\
        return;\
    }\
    PdfViewState *__var = BufferView_GetPdfView(bView);\
    if(!pdfView)\
        return;

void AppCommandOpenFile();
void AppCommandSwitchFont();
void AppCommandListFunctions();
void AppCommandSwapToBuildBuffer();
DisplayWindow *Graphics_GetGlobalWindow();
OpenGLState *Graphics_GetGlobalContext();

void AppViewerNextPage(){
    ViewerSanityCheckRet(pdfView);
    PdfView_NextPage(pdfView);
}

void AppViewerPreviousPage(){
    ViewerSanityCheckRet(pdfView);
    PdfView_PreviousPage(pdfView);
}

void AppViewerIncreaseZoom(){
    ViewerSanityCheckRet(pdfView);
    PdfView_IncreaseZoomLevel(pdfView);
}

void AppViewerDecreaseZoom(){
    ViewerSanityCheckRet(pdfView);
    PdfView_DecreaseZoomLevel(pdfView);
}

void AppViewerZoomReset(){
    ViewerSanityCheckRet(pdfView);
    PdfView_ResetZoom(pdfView);
}

void AppViewerHandleScroll(int is_up, void *){
    int x = 0, y = 0;
    DisplayWindow *window = Graphics_GetGlobalWindow();
    OpenGLState *state = Graphics_GetGlobalContext();
    GetLastRecordedMousePosition(window, &x, &y);

    View *view = AppGetViewAt(x, state->height - y);
    if(!view)
        return;

    BufferView *bView = View_GetBufferView(view);
    if(View_GetState(view) != View_ImageDisplay ||
       BufferView_GetViewType(bView) != ImageView)
    {
        return;
    }

    PdfViewState *pdfView = BufferView_GetPdfView(bView);
    if(!pdfView)
        return;

    int keyControl = KeyboardGetKeyState(Key_LeftControl);
    if(keyControl == KEYBOARD_EVENT_PRESS || keyControl == KEYBOARD_EVENT_REPEAT){
        if(is_up)
            PdfView_IncreaseZoomLevel(pdfView);
        else
            PdfView_DecreaseZoomLevel(pdfView);
    }else{
        vec2f dir(0.f, SoftDx);
        if(!is_up)
            dir.y = -SoftDx;

        vec2f center = PdfView_GetZoomCenter(pdfView) + dir;
        center = Clamp(center, vec2f(0.f), vec2f(1.f));

        if(PdfView_CanMoveTo(pdfView, center)){
            PdfView_SetZoomCenter(pdfView, center);
        }
    }
}

void AppViewerReloadCmd(){
    View *view = AppGetActiveView();
    BufferView *bView = View_GetBufferView(view);
    if(View_GetState(view) != View_ImageDisplay ||
        BufferView_GetViewType(bView) != ImageView)
    {
        return;
    }

    PdfViewState *pdfView = BufferView_GetPdfView(&view->bufferView);
    if(!pdfView){
        return;
    }

    PdfViewGraphicState state = PdfView_GetGraphicsState(pdfView);
    if(PdfView_Reload(&pdfView)){
        PdfView_SetGraphicsState(pdfView, state);
        ImageRenderer &renderer = view->bufferView.imgRenderer;
        renderer.BeginTransition(2);
    }
}

BindingMap *InitializeViewerBindings(){
    DisplayWindow *window = Graphics_GetGlobalWindow();
    BindingMap *mapping = KeyboardCreateMapping();
    RegisterRepeatableEvent(mapping, AppViewerReloadCmd, Key_LeftControl, Key_A);
    RegisterRepeatableEvent(mapping, AppViewerNextPage, Key_Right);
    RegisterRepeatableEvent(mapping, AppViewerPreviousPage, Key_Left);
    RegisterRepeatableEvent(mapping, AppViewerIncreaseZoom, Key_LeftControl, Key_Equal);
    RegisterRepeatableEvent(mapping, AppViewerDecreaseZoom, Key_LeftControl, Key_Minus);
    RegisterRepeatableEvent(mapping, AppViewerZoomReset, Key_LeftControl, Key_R);

    // NOTE: Default ones
    RegisterRepeatableEvent(mapping, AppCommandSwapView, Key_LeftAlt, Key_W);
    RegisterRepeatableEvent(mapping, AppCommandSwapView, Key_RightAlt, Key_W);
    RegisterRepeatableEvent(mapping, AppDefaultReturn, Key_Escape);
    RegisterRepeatableEvent(mapping, AppCommandOpenFile, Key_LeftAlt, Key_F);
    RegisterRepeatableEvent(mapping, AppCommandKillView, Key_LeftControl, Key_LeftAlt, Key_K);
    RegisterRepeatableEvent(mapping, AppCommandKillBuffer, Key_LeftControl, Key_K);
    RegisterRepeatableEvent(mapping, AppCommandSwitchTheme, Key_LeftControl, Key_T);
    RegisterRepeatableEvent(mapping, AppCommandSwitchFont, Key_LeftControl, Key_F);
    RegisterRepeatableEvent(mapping, AppCommandListFunctions, Key_RightControl, Key_F);
    RegisterRepeatableEvent(mapping, AppCommandSwitchBuffer, Key_LeftAlt, Key_B);
    RegisterRepeatableEvent(mapping, AppCommandSplitHorizontal, Key_LeftAlt, Key_LeftControl, Key_H);
    RegisterRepeatableEvent(mapping, AppCommandSplitVertical, Key_LeftAlt, Key_LeftControl, Key_V);
    RegisterRepeatableEvent(mapping, AppCommandSwapToBuildBuffer, Key_LeftControl, Key_M);
    RegisterRepeatableEvent(mapping, AppCommandQueryBarInteractiveCommand,
                            Key_LeftControl, Key_Semicolon);

    RegisterOnScrollCallback(window, AppViewerHandleScroll, nullptr);
    return mapping;
}