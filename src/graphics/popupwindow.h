/* date = June 9th 2022 19:44 */
#pragma once
#include <widgets.h>

/*
* Implement basic popup window for displaying warnings and errors.
*/
class PopupWindow : public WidgetWindow{
    public:

    static uint kButtonOK;
    static uint kButtonCancel;
    static uint kButtonAll;

    /* Buttons for OK and Cancel */
    ButtonWidget okButton, cancelButton;
    /* Texture for displaying informaton icon */
    ImageTextureWidget iconImage;
    /* Text for title */
    TextWidget title;
    /* Text explaining the error/warning */
    TextWidget text;
    /* Current flags for displaying buttons */
    uint buttonFlags;

    /* Button callbacks */
    std::function<void(void)> okButtonCallback;
    std::function<void(void)> cancelButtonCallback;

    PopupWindow();

    /* Set the value for configurable stuff */
    void SetText(std::string message);
    void SetTitle(std::string title);
    void SetButtonFlags(uint flags);

    /* Routines for handling callbacks on buttons */
    void SetOKButtonCallback(std::function<void(void)> callback){
        okButtonCallback = callback;
    }

    void SetCancelButtonCallback(std::function<void(void)> callback){
        cancelButtonCallback = callback;
    }

    void ReleaseButtonSlots();
};
