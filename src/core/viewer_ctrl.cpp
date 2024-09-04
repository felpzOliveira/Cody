#include <viewer_ctrl.h>
#include <keyboard.h>
#include <app.h>
#include <geometry.h>
#include <pdfview.h>

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

PdfViewState *GetActivePdfView(){
    PdfViewState *pview = nullptr;
    BufferView *bView = nullptr;
#if 0
    int x = 0, y = 0;
    DisplayWindow *window = Graphics_GetGlobalWindow();
    OpenGLState *state = Graphics_GetGlobalContext();
    GetLastRecordedMousePosition(window, &x, &y);

    View *view = AppGetViewAt(x, state->height - y);
#else
    View *view = AppGetActiveView();
#endif
    if(!view)
        goto __ret;

    bView = View_GetBufferView(view);
    if(View_GetState(view) != View_ImageDisplay ||
       BufferView_GetViewType(bView) != ImageView)
    {
        goto __ret;
    }

    pview = BufferView_GetPdfView(bView);
__ret:
    return pview;
}

void AppViewerHandleScroll(int is_up, void *){
    PdfViewState *pdfView = GetActivePdfView();
    if(!pdfView)
        return;

    int keyControl = KeyboardGetKeyState(Key_LeftControl);
    int keyAlt     = KeyboardGetKeyState(Key_LeftAlt);
    int keyShift   = KeyboardGetKeyState(Key_LeftShift);
    int n = PdfView_EstimateScrollPage(pdfView);
    int totalPages = PdfView_GetNumPages(pdfView);
    if(keyControl == KEYBOARD_EVENT_PRESS || keyControl == KEYBOARD_EVENT_REPEAT){
        if(is_up)
            PdfView_IncreaseZoomLevel(pdfView);
        else
            PdfView_DecreaseZoomLevel(pdfView);
    }else if(keyAlt == KEYBOARD_EVENT_PRESS || keyAlt == KEYBOARD_EVENT_REPEAT){
        PdfScrollState *scroll = PdfView_GetScroll(pdfView);
        if(is_up){
            if(n > 0)
                scroll->page_off -= 1;
            //PdfView_PreviousPage(pdfView);
        }else{
            if(n < totalPages)
                scroll->page_off += 1;
            //PdfView_NextPage(pdfView);
        }
        //PdfView_ResetZoom(pdfView);
    }else{
        Float delta = SoftDx;
        if(keyShift == KEYBOARD_EVENT_PRESS || keyShift == KEYBOARD_EVENT_REPEAT)
            delta *= 3.f;

        vec2f dir(0.f, delta);
        if(!is_up)
            dir.y = -delta;

        vec2f oldCenter = PdfView_GetZoomCenter(pdfView);
        vec2f center = oldCenter + dir;

        if(n == 0){
            Float y = Clamp(center.y, 0, 1);
            if(!PdfView_AcceptByUpper(pdfView, y))
                return;
        }

        if(n == totalPages-1){
            Float y = Clamp(center.y, 0, 1);
            if(!PdfView_AcceptByLower(pdfView, y))
                return;
        }

        if(center.y < 0){
            PdfView_NextPage(pdfView);
            center.y += 1;
        }else if(center.y > 1){
            PdfView_PreviousPage(pdfView);
            center.y -= 1;
        }

        PdfView_SetZoomCenter(pdfView, center);
    }

}

void ViewerRawKeyCallback(int rawKeyCode){
    if(TranslateKey(rawKeyCode) != Key_LeftAlt)
        return;

    PdfViewState *pdfView = GetActivePdfView();

    if(!pdfView)
        return;

    PdfScrollState *scroll = PdfView_GetScroll(pdfView);
    int keyAlt = KeyboardGetKeyState(Key_LeftAlt);
    if(keyAlt == KEYBOARD_EVENT_PRESS){
        scroll->page_off = 0;
        scroll->active = 1;
    }else if(keyAlt == KEYBOARD_EVENT_RELEASE){
        PdfView_JumpNPages(pdfView, scroll->page_off);
        scroll->page_off = 0;
        scroll->active = 0;
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
        renderer.BeginTransition(1);
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
    RegisterKeyboardRawEntry(mapping, ViewerRawKeyCallback);

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
