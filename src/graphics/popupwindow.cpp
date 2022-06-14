#include <popupwindow.h>

uint PopupWindow::kButtonOK     = (1 << 0);
uint PopupWindow::kButtonCancel = (1 << 1);
uint PopupWindow::kButtonAll    = (kButtonOK | kButtonCancel);

void PopupwindowEmptyCallback(){ /* Empty callback */ }

void PopupwindowOkButtonCallback(Widget *){
    // TODO
}

void PopupwindowCancelButtonCallback(Widget *){
    // TODO
}

void PopupWindow::ReleaseButtonSlots(){
    okButtonCallback = PopupwindowEmptyCallback;
    cancelButtonCallback = PopupwindowEmptyCallback;
}

void PopupWindow::SetText(std::string message){
    std::vector<std::string> splitted;
    StringSplit(message, splitted, '\n');

    text.Clear();
    for(std::string &str : splitted){
        text.AddTextLine(str);
    }
}

void PopupWindow::SetTitle(std::string str){
    title.Clear();
    title.AddTextLine(str);
}

void PopupWindow::SetButtonFlags(uint flags){
    buttonFlags = flags;
    WidgetWindow::Erase(&okButton);
    WidgetWindow::Erase(&cancelButton);

    // TODO: Need to set geometry
    if(buttonFlags & PopupWindow::kButtonOK){
        WidgetWindow::PushWidget(&okButton);
    }

    if(buttonFlags & PopupWindow::kButtonCancel){
        WidgetWindow::PushWidget(&cancelButton);
    }
}

PopupWindow::PopupWindow() : WidgetWindow(600, 400, "Popup!"){
    okButtonCallback = PopupwindowEmptyCallback;
    cancelButtonCallback = PopupwindowEmptyCallback;
    okButton.AddClickedSlot(Slot<Widget *>(PopupwindowOkButtonCallback));
    cancelButton.AddClickedSlot(Slot<Widget *>(PopupwindowCancelButtonCallback));
    SetButtonFlags(PopupWindow::kButtonAll);
#if 0
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

    //timage.SetGeometry(geometry, vec2f(0.3, 0.95), vec2f(0.5, 0.9));
    //timage.SetTextureId(tid1);

    //tbutton.SetText("SCButton!");
    //tbutton.SetGeometry(geometry, vec2f(0.3, 0.95), vec2f(0.5, 0.9));
    //tbutton.SetTextureId(tid0);

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

    tw.SetGeometry(geometry, vec2f(0.3, 0.95), vec2f(0.5, 0.9));
    tw.AddTextLine("This is an example of a text line 1111111111111.");
    tw.AddTextLine("This is an example of a text line 22.");
    tw.AddTextLine("This is an example of a text line 3.");
    tw.AddTextLine("This is an example of a text line 4.");

    WidgetWindow::PushWidget(&b0);
    WidgetWindow::PushWidget(&b1);
    WidgetWindow::PushWidget(&tw);
    //WidgetWindow::PushWidget(&sc);
#endif
}

