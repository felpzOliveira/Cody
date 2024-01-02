#include <autocomplete.h>
#include <utilities.h>
#include <app.h>

static AutoComplete autoComplete;

void AutoComplete_Next(){
    View *view = AppGetActiveView();
    SelectableList *list = view->autoCompleteList;
    SelectableList_NextItem(list);
}

void AutoComplete_Previous(){
    View *view = AppGetActiveView();
    SelectableList *list = view->autoCompleteList;
    SelectableList_PreviousItem(list, 0);
}

void AutoComplete_Commit(){
    View *view = AppGetActiveView();
    SelectableList *list = view->autoCompleteList;
    int id = SelectableList_GetActiveIndex(list);
    if(id >= 0){
        Buffer *buffer = nullptr;
        SelectableList_GetItem(list, id, &buffer);
        if(buffer){
            char *data = &buffer->data[autoComplete.lastSearchLen];
            AppDefaultEntry(data, buffer->taken - autoComplete.lastSearchLen, nullptr);
            if(autoComplete.lastSearchValue) AllocatorFree(autoComplete.lastSearchValue);
            autoComplete.lastSearchLen = 0;
        }
    }

    AppDefaultReturn();
}

void AutoComplete_Interrupt(){
    AppDefaultReturn();
}

BindingMap *AutoComplete_Initialize(){
    BindingMap *mapping = nullptr;
    autoComplete.lastSearchLen = 0;
    autoComplete.lastSearchValue = nullptr;

    mapping = KeyboardCreateMapping();
    RegisterKeyboardDefaultEntry(mapping, AppDefaultEntry, nullptr);
    RegisterRepeatableEvent(mapping, AppDefaultReturn, Key_Escape);
    RegisterRepeatableEvent(mapping, AppDefaultRemoveOne, Key_Backspace);
    RegisterRepeatableEvent(mapping, AutoComplete_Next, Key_Down);
    RegisterRepeatableEvent(mapping, AutoComplete_Previous, Key_Up);
    RegisterRepeatableEvent(mapping, AutoComplete_Commit, Key_Enter);
    RegisterRepeatableEvent(mapping, AutoComplete_Commit, Key_Tab);

    autoComplete.mapping = mapping;
    autoComplete.root = TRIE_NODE_INITIALIZER;
    EncoderDecoder_InitFor(&autoComplete.root.encoder, ENCODER_DECODER_UTF8);
    return mapping;
}

int AutoComplete_Search(char *value, uint valuelen, SelectableList *list){
    LineBuffer *lineBuffer = SelectableList_GetLineBuffer(list);
    if(lineBuffer == nullptr){
        lineBuffer = AllocatorGetN(LineBuffer, 1);
    }else{
        LineBuffer_Free(lineBuffer);
    }

    LineBuffer_InitBlank(lineBuffer);
    auto push_wd = [&](char *buf, uint len, Trie *) -> void{
        if(len != valuelen)
            LineBuffer_InsertLine(lineBuffer, buf, len);
    };

    Trie_Search(&autoComplete.root, value, valuelen, push_wd);

    if(autoComplete.lastSearchValue){
        AllocatorFree(autoComplete.lastSearchValue);
    }

    autoComplete.lastSearchValue = StringDup(value, valuelen);
    autoComplete.lastSearchLen = valuelen;

    SelectableList_SwapList(list, lineBuffer, 0);
    return lineBuffer->lineCount;
}

void AutoComplete_PushString(char *value, uint valuelen){
    if(valuelen > AutoCompleteMinInsertLen){
        Trie_Insert(&autoComplete.root, value, valuelen);
    }
}

void AutoComplete_Remove(char *value, uint valuelen){
    if(valuelen > AutoCompleteMinInsertLen){
        Trie_Remove(&autoComplete.root, value, valuelen);
    }
}

