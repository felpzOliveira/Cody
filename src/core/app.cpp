#include <app.h>
#include <keyboard.h>
#include <lex.h>
#include <utilities.h>
#include <font.h>
#include <bufferview.h>
#include <display.h>
#include <undo.h>
#include <view.h>
#include <string.h>
#include <file_provider.h>
#include <autocomplete.h>
#include <types.h>
#include <view_tree.h>
#include <control_cmds.h>
#include <parallel.h>
#include <timing.h>
#include <dbgapp.h>
#include <widgets.h>
#include <dbgwidget.h>
#include <storage.h>
#include <modal.h>
#include <functional>
#include <unordered_map>
#include <cryptoutil.h>
#include <rng.h>
#include <pdfview.h>
#include <viewer_ctrl.h>

#define DIRECTION_LEFT  0
#define DIRECTION_UP    1
#define DIRECTION_DOWN  2
#define DIRECTION_RIGHT 3

#define MAX_VIEWS 16

#define CODEVIEW_OR_RET(bview) if(BufferView_GetViewType(bview) != CodeView) return

typedef struct{
    BindingMap *freeTypeMapping;
    BindingMap *queryBarMapping;
    BindingMap *autoCompleteMapping;
    BindingMap *viewerMapping;
    View *activeView;
    int activeId;
    uint dbgHandle;
    uint dbgBkptHandle;
    std::function<void(void)> mouseHook;
    std::unordered_map<uint, MouseEventCallback *> mouseEventMap;

    char cwd[PATH_MAX];
    Geometry currentGeometry;
    Float currentLineHeight;
    bool hasDelayedCall;
    std::function<void(void)> delayedCall;
    QueryBarHistory queryBarHistory;
    std::vector<DbgBkpt> dbgBreaks;
}App;

static App appContext = {
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};

AppConfig appGlobalConfig;

void AppInitializeFreeTypingBindings();
void AppInitializeQueryBarBindings();
void AppInitializeViewerBindings();
void AppCommandAutoComplete();
void AppCommandFreeTypingArrows(int direction);
void AppCommandQueryBarCommit();

void AppEmptyMouseHook(){}

void AppSetViewingGeometry(Geometry geometry, Float lineHeight){
    Float w = (Float)(geometry.upper.x - geometry.lower.x);
    Float h = (Float)(geometry.upper.y - geometry.lower.y);
    auto fn = [&](ViewNode *node) -> int{
        View *view = node->view;
        if(view){
            uint x0 = (uint)(w * view->geometry.extensionX.x);
            uint x1 = (uint)(w * view->geometry.extensionX.y);
            uint y0 = (uint)(h * view->geometry.extensionY.x);
            uint y1 = (uint)(h * view->geometry.extensionY.y);

            Geometry targetGeo;
            targetGeo.lower = vec2ui(x0, y0);
            targetGeo.upper = vec2ui(x1, y1);
            targetGeo.extensionX = view->geometry.extensionX;
            targetGeo.extensionY = view->geometry.extensionY;
            View_SetGeometry(view, targetGeo, lineHeight);
            node->geometry = targetGeo;
        }

        return 0;
    };

    ViewTree_ForAllViews(fn);
    appContext.currentGeometry = geometry;
    appContext.currentLineHeight = lineHeight;
}

Geometry AppGetScreenGeometry(Float *lineHeight){
    *lineHeight = appContext.currentLineHeight;
    return appContext.currentGeometry;
}

BindingMap *AppGetFreetypingBinding(){
    return appContext.freeTypeMapping;
}

void AppSetDelayedCall(std::function<void(void)> fn){
    appContext.delayedCall = fn;
    appContext.hasDelayedCall = true;
}

void AppSetCursorStyle(CursorStyle style){
    appGlobalConfig.cStyle = style;
}

CursorStyle AppGetCursorStyle(){
    return appGlobalConfig.cStyle;
}

bool App_IsStateWrite(){
    return !Dbg_IsRunning();
}

int AppGetPathCompression(){
    return appGlobalConfig.pathCompression;
}

void AppSetPathCompression(int value){
    appGlobalConfig.pathCompression = value;
}

char AppGetInvalidSpaceChar(){
    if(appGlobalConfig.useTabs) return ' ';
    return '\t';
}

int AppGetDisplayWrongIdent(){
    return appGlobalConfig.displayWrongIdent;
}

void AppSetDisplayWrongIdent(int should_display){
    appGlobalConfig.displayWrongIdent = should_display;
}

void AppSetRenderViewIndices(int should_render){
    appGlobalConfig.displayViewIndices = should_render;
}

int AppGetRenderViewIndices(){
    return appGlobalConfig.displayViewIndices;
}

uint AppGetFontSize(){
    return appGlobalConfig.defaultFontSize;
}

void AppSetFontSize(uint size){
    /*
    * Small check: make sure whatever we are doing we are not breaking the
    *              interface in geometry terms.
    */
    View *view = AppGetActiveView();
    appGlobalConfig.defaultFontSize = Min(Max(1, size), view->geometry.Height()-5);
    Graphics_SetDefaultFontSize(appGlobalConfig.defaultFontSize);
    /*
    * Now we need to pass the line height information for all views in order to sync
    * line viewing range computation and scrolling otherwise everything will get clipped.
    */
    uint lineHeight = Graphics_GetDefaultLineHeight();
    auto fn = [&](ViewNode *node) -> int{
        View *view = node->view;
        if(view){
            View_SetGeometry(view, view->geometry, lineHeight);
        }
        return 0;
    };

    ViewTree_ForAllViews(fn);
}

void AppEarlyInitialize(bool use_tabs){
    View *view = nullptr;
    StorageDevice *storage = FetchStorageDevice();
    srand(time(0));
    // TODO: Configuration file
    appGlobalConfig.tabLength = 4;
    appGlobalConfig.pathCompression = -1;
    appGlobalConfig.displayWrongIdent = 1;
    appGlobalConfig.displayViewIndices = 0;
    appGlobalConfig.useTabs = use_tabs ? 1 : 0;
    appGlobalConfig.defaultFontSize = 18;
    appGlobalConfig.cStyle = CURSOR_RECT;
    appGlobalConfig.autoCompleteSize = 0;

    ViewTree_Initialize();
    FileProvider_Initialize();

    ViewNode *node = ViewTree_GetCurrent();
    view = node->view;

    view->geometry.lower = vec2ui();
    appContext.activeView = view;
    appContext.hasDelayedCall = false;

    View_SetActive(appContext.activeView, 1);

    storage->GetWorkingDirectory(appContext.cwd, PATH_MAX);

    appContext.autoCompleteMapping = AutoComplete_Initialize();
    std::string dir(appContext.cwd);
    if(dir[dir.size()-1] != '/' && dir[dir.size()-1] != '\\'){
        dir += SEPARATOR_STRING;
    }
    dir += ".cody";
    appGlobalConfig.configFolder = dir;
    appGlobalConfig.rootFolder = appContext.cwd;
    appGlobalConfig.configFile = dir + std::string(SEPARATOR_STRING ".config");

    // TODO: Configurable number of entries?/init function?
    appContext.queryBarHistory.history = CircularStack_Create<QueryBarHistoryItem>(64);
    QueryBarHistory_DetachedLoad(&appContext.queryBarHistory,
                                 QueryBarHistory_GetPath().c_str());

    BaseCommand_InitializeCommandMap();
}

void AppClearHistory(){
    FileHandle handle;
    StorageDevice *device = FetchStorageDevice();
    if(device->StreamWriteStart(&handle, QueryBarHistory_GetPath().c_str())){
        device->StreamFinish(&handle);
    }

    CircularStack_Clear<QueryBarHistoryItem>(appContext.queryBarHistory.history);
}

void AppReloadHistory(){
    std::string path = QueryBarHistory_GetPath();
    QueryBarHistory_DetachedLoad(&appContext.queryBarHistory, path.c_str());
}

int AppGetTabLength(int *using_tab){
    if(using_tab){
        *using_tab = appGlobalConfig.useTabs;
    }

    return appGlobalConfig.tabLength;
}

void AppAddStoredFile(std::string basePath){
    std::string fullPath = appGlobalConfig.rootFolder + std::string(SEPARATOR_STRING) + basePath;
    appGlobalConfig.filesStored.push_back(fullPath);
}

int AppIsStoredFile(std::string path){
    for(std::string &s : appGlobalConfig.filesStored){
        if(s == path) return 1;
    }
    return false;
}

std::string AppGetConfigFilePath(){
    return appGlobalConfig.configFile;
}

std::string AppGetConfigDirectory(){
    return std::string(appGlobalConfig.configFolder);
}

std::string AppGetRootDirectory(){
    return std::string(appGlobalConfig.rootFolder);
}

char *AppGetContextDirectory(){
    return &appContext.cwd[0];
}

View *AppGetActiveView(){
    return appContext.activeView;
}

BufferView *AppGetActiveBufferView(){
    return &appContext.activeView->bufferView;
}

QueryBarHistory *AppGetQueryBarHistory(){
    return &appContext.queryBarHistory;
}

void AppResetBufferViewRangeVisible(BufferView *bview){
    BufferView_SetRangeVisible(bview, 0);
    BufferView_SetMousePressed(bview, 0);
}

void AppResetBufferViewRangeVisible(View *view){
    BufferView *bv = View_GetBufferView(view);
    AppResetBufferViewRangeVisible(bv);
}

void AppOnTypeChange(){
    ClearBuildErrors();
}

void AppSetBindingsForState(ViewState state, ViewType type){
    switch(state){
        case View_SelectableList:
        case View_QueryBar: KeyboardSetActiveMapping(appContext.queryBarMapping); break;
        case View_FreeTyping: KeyboardSetActiveMapping(appContext.freeTypeMapping); break;
        case View_AutoComplete: KeyboardSetActiveMapping(appContext.autoCompleteMapping); break;
        case View_ImageDisplay: KeyboardSetActiveMapping(appContext.viewerMapping); break;
        default:{
            AssertErr(0, "Not implemented binding");
        }
    }

    // hmmm, checking for Dbg_IsRunning should be reliable but I wish we
    // could mark the view as in debug mode ... because doing this will
    // lock all writes even if it is not *in debug* but again, the only
    // way to check if a file is under debug is by querying dbg and that
    // might be way to expensive, so for now let's keep it this way.
    if(type == DbgView || (type == CodeView && Dbg_IsRunning())){
        DbgApp_TakeKeyboard();
    }

    if(appContext.hasDelayedCall){
        appContext.delayedCall();
        appContext.hasDelayedCall = false;
    }
}

void AppSetActiveView(View *view, bool force_binding){
    AssertA(view != nullptr, "Invalid view");
    if(appContext.activeView){
        View_SetActive(appContext.activeView, 0);
    }
    appContext.activeView = view;
    View_SetActive(appContext.activeView, 1);
    if(force_binding){
        BufferView *bView = View_GetBufferView(appContext.activeView);
        ViewState state = View_GetState(appContext.activeView);
        ViewType type = BufferView_GetViewType(bView);
        AppSetBindingsForState(state, type);
    }
}

void AppUpdateViews(){
    ViewTreeIterator iterator;
    ViewTree_Begin(&iterator);
    while(iterator.value){
        View *view = iterator.value->view;

        BufferView *bView = &view->bufferView;
        BufferView_Synchronize(bView);

        ViewTree_Next(&iterator);
    }
}

View *AppSearchView(LineBuffer *lineBuffer){
    View *foundView = nullptr;
    ViewTreeIterator iterator;
    ViewTree_Begin(&iterator);
    while(iterator.value){
        View *view = iterator.value->view;
        LineBuffer *lb = BufferView_GetLineBuffer(&view->bufferView);
        if(lb == lineBuffer){
            foundView = view;
            break;
        }

        ViewTree_Next(&iterator);
    }

    return foundView;
}

void AppRestoreAllViews(){
    ViewTreeIterator iterator;
    ViewTree_Begin(&iterator);
    while(iterator.value){
        View *view = iterator.value->view;
        BufferView *bView = &view->bufferView;
        ViewType type = BufferView_GetViewType(bView);
        if(type == GitDiffView){
            //AppCommandGitDiff(bView);
        }

        ViewTree_Next(&iterator);
    }
}

void AppRestoreCurrentBufferViewState(){
    BufferView *view = AppGetActiveBufferView();
    if(view){
        ViewType type = BufferView_GetViewType(view);
        // TODO: Add as needed
        if(type == GitDiffView){
            //AppCommandGitDiffCurrent();
        }
    }
}

uint AppRegisterOnMouseEventCallback(MouseEventCallback *cb){
    uint handle = Bad_RNG16();
    bool got_handle = true;
    do{
        got_handle = true;
        if(appContext.mouseEventMap.find(handle) != appContext.mouseEventMap.end()){
            got_handle = false;
            handle = Bad_RNG16();
        }
    }while(!got_handle);

    appContext.mouseEventMap[handle] = cb;
    return handle;
}

void AppReleaseOnMouseEventCallback(uint handle){
    appContext.mouseEventMap.erase(handle);
}

static int lastMouseState = 0;
void DoPdfViewMouseOperations(int x, int y, OpenGLState *state, int mouseState){
    View *view = AppGetActiveView();
    vec2ui mouse(x, state->height - y);
    BufferView *bView = View_GetBufferView(view);
    if(View_GetState(view) == View_ImageDisplay){
        Geometry geo;
        BufferView_GetGeometry(bView, &geo);
        if(Geometry_IsPointInside(&geo, mouse)){
            PdfViewState *pdfView = BufferView_GetPdfView(bView);
            if(pdfView){
                BufferView_GetGeometry(bView, &geo);
                PdfView_MouseMotion(pdfView, mouse, mouseState, &geo);
            }
        }
    }
}

void AppOnMouseMotion(int x, int y, OpenGLState *state, bool press){
    View *oview = AppGetActiveView();
    View *view = oview;
    vec2ui mouse(x, state->height - y);
    BufferView *bView = View_GetBufferView(view);

    if(press){
        mouse = AppActivateViewAt(x, state->height - y);
        view = AppGetActiveView();
        NullRet(view);
        if(view != oview){
            AppResetBufferViewRangeVisible(oview);
        }

        for(auto it : appContext.mouseEventMap){
            it.second();
        }
    }else{
        Geometry geo;
        BufferView *bView = View_GetBufferView(view);

        if(!BufferView_GetMousePressed(bView)) return;

        BufferView_GetGeometry(bView, &geo);
        if(!Geometry_IsPointInside(&geo, mouse)) return;

        mouse = mouse - geo.lower;
    }

    ViewState vstate = View_GetState(view);
    BufferView *bufferView = View_GetBufferView(view);
    uint dy = view->geometry.upper.y - view->geometry.lower.y;

    if(vstate == View_FreeTyping){
        int lineNo = BufferView_ComputeTextLine(bufferView, dy - mouse.y,
                                                view->descLocation);
        if(lineNo < 0) return;

        lineNo = Clamp((uint)lineNo, (uint)0, BufferView_GetLineCount(bufferView)-1);

        uint colNo = 0;
        Buffer *buffer = BufferView_GetBufferAt(bufferView, (uint)lineNo);
        Transform transform = View_GetTranslateTransform(view);
        EncoderDecoder *encoder = LineBuffer_GetEncoderDecoder(bufferView->lineBuffer);
        x = ScreenToTransform(mouse.x, transform) - bufferView->lineOffset;
        if(x > 0){
            colNo = fonsComputeStringOffsetCount(state->font.fsContext,
                                                 buffer->data, x, (void *)encoder);

            colNo = Buffer_PositionTabCompensation(buffer, colNo, -1);

            BufferView_CursorToPosition(bufferView, (uint)lineNo, colNo);
            if(press){
                BufferView_GhostCursorFollow(bufferView);
                BufferView_SetRangeVisible(bufferView, 0);
                BufferView_SetMousePressed(bufferView, 1);
            }else{
                vec2ui ghost = BufferView_GetGhostCursorPosition(bufferView);
                if(ghost.x != (uint)lineNo || ghost.y != colNo){
                    BufferView_SetRangeVisible(bufferView, 1);
                }
            }
        }
    }
}

void AppHandleMouseMotion(int x, int y, OpenGLState *state){
    AppOnMouseMotion(x, y, state, false);
    DoPdfViewMouseOperations(x, y, state, lastMouseState);
#if 0
    vec2ui mouse(x, state->height - y);
    View *view = AppGetActiveView();
    if(Geometry_IsPointInside(&view->geometry, mouse)){
        vec2ui relative = mouse - view->geometry.lower;
        BufferView *bufferView = View_GetBufferView(view);
        uint dy = view->geometry.upper.y - view->geometry.lower.y;
        int lineNo = BufferView_ComputeTextLine(bufferView, dy - relative.y,
                                                view->descLocation);

        if(lineNo < 0) return;

        lineNo = Clamp((uint)lineNo, (uint)0, BufferView_GetLineCount(bufferView)-1);
        Buffer *buffer = BufferView_GetBufferAt(bufferView, (uint)lineNo);
        int x = ScreenToGL(relative.x, state) - bufferView->lineOffset;
        if(x < 0) return;

        uint colNo = fonsComputeStringOffsetCount(state->font.fsContext, buffer->data, x);
        colNo = Buffer_PositionTabCompensation(buffer, colNo, -1);

        uint p = Buffer_Utf8RawPositionToPosition(buffer, colNo);
        uint tid = Buffer_GetTokenAt(buffer, p);
        if(tid < buffer->tokenCount){
            Token *token = &buffer->tokens[tid];
            std::string tvalue(&buffer->data[token->position], token->size);
            printf("Value = %s\n", tvalue.c_str());
        }
    }
#endif
}

static void AppPushBreakpoint(){
    // for now lets handle debugger stuff
    if(Dbg_IsRunning()){
        View *view = AppGetActiveView();
        // the debugger is alive, lets add a breakpoint
        BufferView *bView = View_GetBufferView(view);
        LineBuffer *lineBuffer = BufferView_GetLineBuffer(bView);
        NullRet(lineBuffer);
        vec2ui cursor = BufferView_GetCursorPosition(bView);
        uint lineno = cursor.x + 1;
        printf("breaking at %s:%d\n", LineBuffer_GetStoragePath(lineBuffer), lineno);

        DbgApp_Break(LineBuffer_GetStoragePath(lineBuffer), lineno);
    }
}

std::optional<vec2ui> AppGetTextPosition(int x, int y, View *view,
                                         OpenGLState *state)
{
    std::optional<vec2ui> res = {};
    Geometry geo;
    vec2ui pos(x, state->height - y);
    BufferView *bufferView = View_GetBufferView(view);
    BufferView_GetGeometry(bufferView, &geo);

    pos = pos - geo.lower;

    uint dy = view->geometry.upper.y - view->geometry.lower.y;
    int lineNo = BufferView_ComputeTextLine(bufferView, dy - pos.y,
                                            view->descLocation);
    if(lineNo < 0) return res;
    lineNo = Clamp((uint)lineNo, (uint)0,
                    BufferView_GetLineCount(bufferView)-1);
    uint colNo = 0;
    Buffer *buffer = BufferView_GetBufferAt(bufferView, (uint)lineNo);
    Float x0 = ScreenToGL(pos.x, state) - bufferView->lineOffset;

    EncoderDecoder *encoder = LineBuffer_GetEncoderDecoder(bufferView->lineBuffer);

    if(x0 > 0){
        colNo = fonsComputeStringOffsetCount(state->font.fsContext, buffer->data,
                                             x0, (void *)encoder);
        colNo = Buffer_PositionTabCompensation(buffer, colNo, -1);
        res = std::optional<vec2ui>(vec2ui(lineNo, colNo));
    }
    return res;
}

void AppSelectCurrentToken(int x, int y, OpenGLState *state){
    View *view = AppGetActiveView();
    ViewState vstate = View_GetState(view);
    if(vstate == View_FreeTyping){
        BufferView *bufferView = View_GetBufferView(view);
        std::optional<vec2ui> res = AppGetTextPosition(x, y, view, state);
        if(!res) return;
        vec2ui loc = res.value();

        Buffer *buffer = BufferView_GetBufferAt(bufferView, loc.x);
        NullRet(buffer);

        EncoderDecoder *encoder = LineBuffer_GetEncoderDecoder(bufferView->lineBuffer);

        uint tid = Buffer_GetTokenAt(buffer, loc.y, encoder);

        if(tid < buffer->tokenCount){
            Token *token = &buffer->tokens[tid];
            uint n0 = token->position;
            uint n1 = token->position + token->size;
            BufferView_CursorToPosition(bufferView, loc.x, n0);
            BufferView_GhostCursorFollow(bufferView);
            BufferView_CursorToPosition(bufferView, loc.x, n1);
            BufferView_SetRangeVisible(bufferView, 1);
        }
    }
}

void AppHandleDoubleClick(int x, int y, OpenGLState *state){
    /*
    * Double clicks are interesting. Because we do not filter
    * out the single click the view has already been activate at
    * coordinates (x, y) for the single click handler, this means
    * we don't actually have much work to do here.
    */
    if(Dbg_IsRunning()){
        AppPushBreakpoint();
    }else{
        // TODO: While this seems to be working, I don't actually like it.
        //AppSelectCurrentToken(x, y, state);
    }
}

static void CursorToRegion(BufferView *bView, uint x){
    uint s = BufferView_GetMaximumLineView(bView);
    int in = x + s;
    s = Clamp(in - 8, 0, BufferView_GetLineCount(bView)-1);
    // TODO: This is very ineficient as we have to do scroll
    //       range adjustment twice. It would be best if we
    //       could set them all together with a call such as:
    //            BufferView_SetViewDisplay(...)
    //       and configure cursor + viewing rectangle in one call.
    //       But I'm not sure how much worse it is, for now looks ok.

    // trigger range change, to 'drag' the viewing rectangle
    BufferView_CursorToPosition(bView, s, 0);
    // restore view, set actual cursor location
    BufferView_CursorToPosition(bView, x, 0);
}

void AppJumpToNextError(){
    std::optional<BuildError> err = NextBuildError();
    if(err){
        int fileType = -1;
        LineBuffer *lBuffer = nullptr;
        BuildError bErr = err.value();
        View *view = AppGetActiveView();
        int rv = FileProvider_FindByPath(&lBuffer, (char *)bErr.file.c_str(),
                                         bErr.file.size(), nullptr);
        if(!rv){
            FileProvider_Load((char *)bErr.file.c_str(), bErr.file.size(),
                              fileType, &lBuffer, false);
        }

        if(lBuffer){
            BufferView *bView = View_GetBufferView(view);
            BufferView_SwapBuffer(bView, lBuffer, CodeView);
            CursorToRegion(bView, bErr.line < 1 ? 0 : bErr.line-1);
        }else{
            printf("Could not load file\n");
        }
    }
}

void AppHandleMouseClick(int x, int y, OpenGLState *state){
    View *oview = AppGetActiveView();
    View *view = oview;
    vec2ui mouse = AppActivateViewAt(x, state->height - y);
    view = AppGetActiveView();
    NullRet(view);
    if(view != oview){
        AppResetBufferViewRangeVisible(oview);
    }

    ViewState vstate = View_GetState(view);
    uint dy = view->geometry.upper.y - view->geometry.lower.y;
    // account for view transform
    Transform transform = View_GetTranslateTransform(view);

    if(vstate == View_SelectableList){
        SelectableList *list = &view->selectableList;
        y = dy - mouse.y;
        y = ScreenToGL(y, state);
        if(view->descLocation == DescriptionTop){
            y -= state->font.fontMath.fontSizeAtRenderCall;
        }
        vec2i item = Graphics_ComputeSelectableListItem(state, y, view);
        if(item.y >= 0){
            SelectableList_SetItem(list, item.y);
            AppCommandQueryBarCommit();
        }
    }else if(vstate == View_FreeTyping){
        // becase this is also a release event we can actually do this
        BufferView *bufferView = View_GetBufferView(view);
        vec2ui ghost = BufferView_GetGhostCursorPosition(bufferView);
        int lineNo = BufferView_ComputeTextLine(bufferView, dy - mouse.y,
                                                view->descLocation);
        if(lineNo < 0) return;

        lineNo = Clamp((uint)lineNo, (uint)0, BufferView_GetLineCount(bufferView)-1);

        uint colNo = 0;
        EncoderDecoder *encoder = LineBuffer_GetEncoderDecoder(bufferView->lineBuffer);
        Buffer *buffer = BufferView_GetBufferAt(bufferView, (uint)lineNo);
        Float x = ScreenToTransform(mouse.x, transform) - bufferView->lineOffset;
        if(x < 0) return;

        colNo = fonsComputeStringOffsetCount(state->font.fsContext, buffer->data,
                                             x, (void *)encoder);

        colNo = Buffer_PositionTabCompensation(buffer, colNo, -1);
        if(ghost.x != (uint)lineNo || ghost.y != colNo){
            BufferView_CursorToPosition(bufferView, (uint)lineNo, colNo);
        }else{
            BufferView_SetRangeVisible(bufferView, 0);
        }

        BufferView_SetMousePressed(bufferView, 0);
    }

    appContext.mouseHook();
}

void AppHandleMousePress(int x, int y, OpenGLState *state){
    AppOnMouseMotion(x, y, state, true);
    appContext.mouseHook();
    lastMouseState = 1;
    DoPdfViewMouseOperations(x, y, state, lastMouseState);
}

void AppHandleMouseReleased(int x, int y, OpenGLState *state){
    lastMouseState = 0;
    DoPdfViewMouseOperations(x, y, state, lastMouseState);
}

void AppHandleMouseScroll(int x, int y, int is_up, OpenGLState *state){
    View *view = AppGetViewAt(x, state->height - y);
    if(view){
        ViewState vstate = View_GetState(view);
        BufferView *bView = View_GetBufferView(view);

        switch(vstate){
            case View_FreeTyping:{
                int scrollRange = is_up ? -5 : 5;
                NullRet(bView);
                NullRet(bView->lineBuffer);
                BufferView_StartScrollViewTransition(bView, scrollRange,
                                                     kTransitionScrollInterval);
                BufferView_SetRangeVisible(bView, 0);
            } break;
            case View_SelectableList:{
                //printf("View_SelectableList\n");
                if(is_up){
                    AppCommandQueryBarPrevious();
                }else{
                    AppCommandQueryBarNext();
                }
            } break;
            case View_QueryBar:{
                //printf("View_QueryBar\n");
            } break;
            default:{
                // nothing
            }
        }

        Timing_Update();
    }

    for(auto it : appContext.mouseEventMap){
        it.second();
    }
}

void AppCommandFreeTypingJumpToDirection(int direction){
    BufferView *bufferView = AppGetActiveBufferView();
    //CODEVIEW_OR_RET(bufferView);
    if(!bufferView->lineBuffer) return;
    BufferView_SetRangeVisible(bufferView, 0);

    Token *token = nullptr;
    EncoderDecoder *encoder = LineBuffer_GetEncoderDecoder(bufferView->lineBuffer);
    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    switch(direction){
        case DIRECTION_LEFT:{ // Move left
            Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
            int tid = BufferView_LocatePreviousCursorToken(bufferView, &token);
            uint targetY = 0;
            if(tid >= 0){
                targetY = token->position;
                if(!Symbol_IsTokenJumpable(token->identifier)){
                    uint rawp = Buffer_Utf8PositionToRawPosition(buffer, cursor.y, nullptr, encoder);
                    targetY = Buffer_FindPreviousSeparator(buffer, rawp);
                }
            }

            cursor.y = Buffer_Utf8RawPositionToPosition(buffer, targetY, encoder);

            BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
        } break;
        case DIRECTION_RIGHT:{ // Move right
            Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
            int tid = BufferView_LocateNextCursorToken(bufferView, &token);
            uint targetY = buffer->taken;
            if(tid >= 0){
                targetY = token->position + token->size;
                if(!Symbol_IsTokenJumpable(token->identifier)){
                    uint rawp = Buffer_Utf8PositionToRawPosition(buffer, cursor.y, nullptr, encoder);
                    targetY = Buffer_FindNextSeparator(buffer, rawp);
                }
            }

            cursor.y = Buffer_Utf8RawPositionToPosition(buffer, targetY, encoder);
            BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
        } break;
        case DIRECTION_UP:{ // Move Up
            if(cursor.x > 0){
                uint line = cursor.x - 1;
                int picked = 0;
                while(picked == 0 && line > 0){
                    Buffer *buffer = BufferView_GetBufferAt(bufferView, line);
                    if(Buffer_IsBlank(buffer)) picked = 1;
                    else line--;
                }

                cursor.y = 0;
                cursor.x = line;
                BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
            }
        } break;
        case DIRECTION_DOWN:{ // Move down
            if(cursor.x < BufferView_GetLineCount(bufferView)-1){
                uint line = cursor.x + 1;
                int picked = 0;
                while(picked == 0 && line < BufferView_GetLineCount(bufferView)-1){
                    Buffer *buffer = BufferView_GetBufferAt(bufferView, line);
                    if(Buffer_IsBlank(buffer)) picked = 1;
                    else line++;
                }

                cursor.y = 0;
                cursor.x = line;
                BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
            }
        } break;

        default: AssertA(0, "Invalid direction given");
    }
}

void AppCommandFreeTypingArrows(int direction){
    BufferView *bufferView = AppGetActiveBufferView();
    //CODEVIEW_OR_RET(bufferView);
    if(!bufferView->lineBuffer) return;
    BufferView_SetRangeVisible(bufferView, 0);

    EncoderDecoder *encoder = LineBuffer_GetEncoderDecoder(bufferView->lineBuffer);
    uint lineCount = BufferView_GetLineCount(bufferView);
    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    int tabdir = -1;
    switch(direction){
        case DIRECTION_LEFT:{ // Move left
            if(cursor.y == 0){ // move up
                cursor.x = cursor.x > 0 ? cursor.x - 1 : 0;
            }else{
                cursor.y -= 1;
            }

            Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
            cursor.y = Clamp(cursor.y, (uint)0, buffer->count);
        } break;
        case DIRECTION_UP:{ // Move Up
            cursor.x = cursor.x > 0 ? cursor.x - 1 : 0;
            Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
            int n = Buffer_FindFirstNonEmptyToken(buffer);
            n = n < 0 ? 0 : buffer->tokens[n].position;
            n = Buffer_Utf8RawPositionToPosition(buffer, n, encoder);
            cursor.y = Clamp(cursor.y, (uint)n, buffer->count);
        } break;
        case DIRECTION_DOWN:{ // Move down
            cursor.x = Clamp(cursor.x+1, (uint)0, lineCount-1);

            Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
            int n = Buffer_FindFirstNonEmptyToken(buffer);
            n = n < 0 ? 0 : buffer->tokens[n].position;
            n = Buffer_Utf8RawPositionToPosition(buffer, n, encoder);
            cursor.y = Clamp(cursor.y, (uint)n, buffer->count);
        } break;

        case DIRECTION_RIGHT:{ // Move right
            Buffer *buffer = LineBuffer_GetBufferAt(bufferView->lineBuffer, cursor.x);
            if(cursor.y >= buffer->count){ // move down
                cursor.x = Clamp(cursor.x+1, (uint)0, lineCount-1);
            }else{
                cursor.y += 1;
            }

            buffer = BufferView_GetBufferAt(bufferView, cursor.x);
            cursor.y = Clamp(cursor.y, (uint)0, buffer->count);
            tabdir = 1;
        } break;

        default: AssertA(0, "Invalid direction given");
    }

    Buffer *nbuffer = BufferView_GetBufferAt(bufferView, cursor.x);
    cursor.y = Buffer_PositionTabCompensation(nbuffer, cursor.y, tabdir);
    BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
}

void RemountTokensBasedOn(BufferView *view, uint base, uint offset=0){
    LineBuffer *lineBuffer = view->lineBuffer;
    Tokenizer *tokenizer = FileProvider_GetLineBufferTokenizer(lineBuffer);
    LineBuffer_ReTokenizeFromBuffer(lineBuffer, tokenizer, base, offset);
}

void AppQueryBarSearchJumpToResult(QueryBar *bar, View *view){
    QueryBarCmdSearch *result = nullptr;
    BufferView *bView = View_GetBufferView(view);
    if(!bView->lineBuffer) return;
    QueryBar_GetSearchResult(bar, &result);

    if(result->valid){
        Buffer *buf = BufferView_GetBufferAt(bView, result->lineNo);
        EncoderDecoder *encoder = LineBuffer_GetEncoderDecoder(bView->lineBuffer);
        // TODO: This is being called even when it is not search operation!
        if(buf){
            uint c = Buffer_Utf8RawPositionToPosition(buf, result->position, encoder);
            BufferView_CursorToPosition(bView, result->lineNo, c);
        }
    }
}

void AppHandleRegionCut(bool toClipboard=true){
    vec2ui start, end;
    uint size = 0;
    char *ptr = nullptr;
    BufferView *view = AppGetActiveBufferView();

    NullRet(view->lineBuffer);
    NullRet(LineBuffer_IsWrittable(view->lineBuffer));
    AppOnTypeChange();

    if(BufferView_GetCursorSelectionRange(view, &start, &end)){
        size = LineBuffer_GetTextFromRange(view->lineBuffer, &ptr, start, end);
        if(toClipboard){
            ClipboardSetString(ptr, size);
        }

        UndoRedoUndoPushInsertBlock(&view->lineBuffer->undoRedo, start, ptr, size);
        AppCommandRemoveTextBlock(view, start, end);

        RemountTokensBasedOn(view, start.x, 1);
        BufferView_CursorToPosition(view, start.x, start.y);
        BufferView_AdjustGhostCursorIfOut(view);
        BufferView_Dirty(view);
        BufferView_SetRangeVisible(view, 0);
    }
}

DisplayWindow *Graphics_GetGlobalWindow();
void AppTerminate(){
    SetWindowShouldClose(Graphics_GetGlobalWindow());
}

void AppDefaultRemoveOne(){
    View *view = AppGetActiveView();
    ViewState state = View_GetState(view);
    BufferView *bufferView = View_GetBufferView(view);

    if(state == View_FreeTyping || state == View_AutoComplete){
        vec2ui cursor = BufferView_GetCursorPosition(bufferView);
        Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);

        NullRet(buffer);
        NullRet(bufferView->lineBuffer);
        NullRet(LineBuffer_IsWrittable(bufferView->lineBuffer));
        AppOnTypeChange();

        if(BufferView_GetRangeVisible(bufferView)){
            // in case we are in a block selection erase the block
            AppHandleRegionCut(false);
            return;
        }

        Tokenizer *tokenizer = FileProvider_GetLineBufferTokenizer(bufferView->lineBuffer);
        SymbolTable *symTable = tokenizer->symbolTable;

        if(cursor.y > 0){
            EncoderDecoder *encoder = LineBuffer_GetEncoderDecoder(bufferView->lineBuffer);
            vec2i id = LineBuffer_GetActiveBuffer(bufferView->lineBuffer);
            if(id.x != (int)cursor.x || id.y != OPERATION_REMOVE_CHAR){
                // user started to edit a different buffer
                UndoRedoUndoPushInsert(&bufferView->lineBuffer->undoRedo, buffer, cursor);
                LineBuffer_SetActiveBuffer(bufferView->lineBuffer,
                                           vec2i((int)cursor.x, OPERATION_REMOVE_CHAR));
            }

            Buffer_EraseSymbols(buffer, symTable);

            // need to compute the position with regards to tab otherwise we won't
            // be able to correctly erase tabs
            uint y = Buffer_PositionTabCompensation(buffer, cursor.y - 1, -1);

            Buffer_RemoveRange(buffer, y, cursor.y, encoder);
            RemountTokensBasedOn(bufferView, cursor.x);
            cursor.y = y;

        }else if(cursor.x > 0){
            int offset = buffer->count;
            Buffer *pBuffer = BufferView_GetBufferAt(bufferView, cursor.x-1);
            Buffer_EraseSymbols(pBuffer, symTable);
            Buffer_EraseSymbols(buffer, symTable);

            LineBuffer_MergeConsecutiveLines(bufferView->lineBuffer, cursor.x-1);
            LineBuffer_SetActiveBuffer(bufferView->lineBuffer,
                                    vec2i(cursor.x, OPERATION_REMOVE_LINE));
            buffer = BufferView_GetBufferAt(bufferView, cursor.x-1);
            RemountTokensBasedOn(bufferView, cursor.x-1);

            cursor.y = buffer->count - offset;
            cursor.x -= 1;
            UndoRedoUndoPushNewLine(&bufferView->lineBuffer->undoRedo, cursor);
        }

        BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
        BufferView_AdjustGhostCursorIfOut(bufferView);
        BufferView_Dirty(bufferView);
        if(state == View_AutoComplete){
            /* help auto-complete better detect interrupt events since it is using
               the default entry/removal routines */
            appGlobalConfig.autoCompleteSize -= 1;
            if(appGlobalConfig.autoCompleteSize <= 0){
                AutoComplete_Interrupt();
                appGlobalConfig.autoCompleteSize = 0;
            }else{
                AppCommandAutoComplete();
            }
        }
    }else if(View_IsQueryBarActive(view)){
        QueryBar *bar = View_GetQueryBar(view);
        if(QueryBar_RemoveOne(bar, view) != 0){
            AppQueryBarSearchJumpToResult(bar, view); // TODO: other choises
        }
    }
}

void AppCommandRemovePreviousToken(){
    Token *token = nullptr;
    BufferView *bufferView = AppGetActiveBufferView();
    if(!bufferView->lineBuffer) return;
    NullRet(LineBuffer_IsWrittable(bufferView->lineBuffer));
    AppOnTypeChange();

    EncoderDecoder *encoder = LineBuffer_GetEncoderDecoder(bufferView->lineBuffer);
    Tokenizer *tokenizer = FileProvider_GetLineBufferTokenizer(bufferView->lineBuffer);
    SymbolTable *symTable = tokenizer->symbolTable;
    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);

    int tid = BufferView_LocatePreviousCursorToken(bufferView, &token);
    if(tid >= 0){
        vec2i id = LineBuffer_GetActiveBuffer(bufferView->lineBuffer);
        if(id.x != (int)cursor.x || id.y != OPERATION_REMOVE_CHAR){
            // user started to edit a different buffer
            UndoRedoUndoPushInsert(&bufferView->lineBuffer->undoRedo, buffer, cursor);
            LineBuffer_SetActiveBuffer(bufferView->lineBuffer,
                                       vec2i((int)cursor.x, OPERATION_REMOVE_CHAR));
        }

        Buffer_EraseSymbols(buffer, symTable);
        uint u8tp = Buffer_Utf8RawPositionToPosition(buffer, token->position, encoder);
        Buffer_RemoveRange(buffer, u8tp, cursor.y, encoder);
        cursor.y = u8tp;
        RemountTokensBasedOn(bufferView, cursor.x);
    }else if(cursor.x > 0){
        int offset = buffer->count;
        Buffer *pBuffer = BufferView_GetBufferAt(bufferView, cursor.x-1);
        Buffer_EraseSymbols(pBuffer, symTable);
        Buffer_EraseSymbols(buffer, symTable);

        LineBuffer_MergeConsecutiveLines(bufferView->lineBuffer, cursor.x-1);
        buffer = BufferView_GetBufferAt(bufferView, cursor.x-1);
        RemountTokensBasedOn(bufferView, cursor.x-1);

        cursor.y = buffer->count - offset;
        cursor.x -= 1;
        UndoRedoUndoPushNewLine(&bufferView->lineBuffer->undoRedo, cursor);
    }
    BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);

    BufferView_AdjustGhostCursorIfOut(bufferView);
    BufferView_Dirty(bufferView);
}

void AppCommandInsertTab(){
    BufferView *bufferView = AppGetActiveBufferView();
    if(!bufferView->lineBuffer) return;
    NullRet(LineBuffer_IsWrittable(bufferView->lineBuffer));
    AppOnTypeChange();

    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
    EncoderDecoder *encoder = LineBuffer_GetEncoderDecoder(bufferView->lineBuffer);
    char spaces[16];
    int offset = 0;

    if(appGlobalConfig.useTabs){
        offset = 1;
        spaces[0] = '\t';
    }else{
        Memset(spaces, ' ', appGlobalConfig.tabLength);
        offset = appGlobalConfig.tabLength;
    }

    vec2i id = LineBuffer_GetActiveBuffer(bufferView->lineBuffer);
    if(id.x != (int)cursor.x || id.y != OPERATION_INSERT_CHAR){
        // user started to edit a different buffer
        UndoRedoUndoPushRemove(&bufferView->lineBuffer->undoRedo, buffer, cursor);
        LineBuffer_SetActiveBuffer(bufferView->lineBuffer,
                                   vec2i((int)cursor.x, OPERATION_INSERT_CHAR));
    }

    Buffer_InsertStringAt(buffer, cursor.y, spaces, offset, encoder);
    RemountTokensBasedOn(bufferView, cursor.x);

    cursor.y += offset;
    BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
    BufferView_Dirty(bufferView);
}

void AppCommandRemoveTextBlock(BufferView *bufferView, vec2ui start, vec2ui end){
    Buffer *buffer = BufferView_GetBufferAt(bufferView, start.x);
    Tokenizer *tokenizer = FileProvider_GetLineBufferTokenizer(bufferView->lineBuffer);
    EncoderDecoder *encoder = LineBuffer_GetEncoderDecoder(bufferView->lineBuffer);
    SymbolTable *symTable = tokenizer->symbolTable;
    if(start.x == end.x){
        Buffer_EraseSymbols(buffer, symTable);
        Buffer_RemoveRange(buffer, start.y, end.y, encoder);
    }else{
        uint rmov = 0;
        Buffer_EraseSymbols(buffer, symTable);
        Buffer_RemoveRange(buffer, start.y, buffer->count, encoder);
        for(uint i = start.x + 1; i < end.x; i++){
            Buffer *b0 = BufferView_GetBufferAt(bufferView, start.x+1);
            Buffer_EraseSymbols(b0, symTable);
            LineBuffer_RemoveLineAt(bufferView->lineBuffer, start.x+1);
            rmov++;
        }

        // remove end now
        buffer = BufferView_GetBufferAt(bufferView, end.x - rmov);
        Buffer_EraseSymbols(buffer, symTable);
        Buffer_RemoveRange(buffer, 0, end.y, encoder);

        // merge start and end
        LineBuffer_MergeConsecutiveLines(bufferView->lineBuffer, start.x);
        BufferView_Dirty(bufferView);
    }
}

vec2ui AppCommandNewLine(BufferView *bufferView, vec2ui at){
    char *lineHelper = nullptr;
    LineBuffer *lineBuffer = BufferView_GetLineBuffer(bufferView);
    Tokenizer *tokenizer = FileProvider_GetLineBufferTokenizer(lineBuffer);
    EncoderDecoder *encoder = LineBuffer_GetEncoderDecoder(lineBuffer);
    SymbolTable *symTable = tokenizer->symbolTable;
    Buffer *buffer = BufferView_GetBufferAt(bufferView, at.x);
    Buffer *bufferp1 = nullptr;
    uint len = 0;

    Buffer_EraseSymbols(buffer, symTable);

    uint s = AppComputeLineIndentLevel(buffer, at.y, encoder);
    uint tid = Buffer_GetTokenAt(buffer, at.y, encoder);
    uint offset = appGlobalConfig.useTabs ? 1 : appGlobalConfig.tabLength;

    len = offset * s;

    // consider the case where the next token is a brace close and the user want to
    // ignore the last indent level
    if(tid < buffer->tokenCount){
        if(buffer->tokens[tid].identifier == TOKEN_ID_BRACE_CLOSE){
            if(len >= offset){
                len -= offset;
            }
        }
    }

    int toNextLine = Max((int)buffer->count - (int)at.y, 0);
    char *dataptr = nullptr;
    if(toNextLine > 0){
        uint p = Buffer_Utf8PositionToRawPosition(buffer, (uint)at.y, nullptr, encoder);
        dataptr = &buffer->data[p];
        toNextLine = buffer->taken - p;
    }

    if(len + toNextLine > 0){
        lineHelper = AllocatorGetN(char, (len + toNextLine) * sizeof(char));
    }

    if(len > 0){
        char indentChar = ' ';
        if(appGlobalConfig.useTabs) indentChar = '\t';
        Memset(lineHelper, indentChar, sizeof(char) * len);
    }

    if(toNextLine > 0){
        Memcpy(&lineHelper[len], dataptr, toNextLine * sizeof(char));
    }

    LineBuffer_InsertLineAt(lineBuffer, at.x+1, lineHelper, len+toNextLine);
    if(toNextLine > 0){
        buffer = BufferView_GetBufferAt(bufferView, at.x);
        Buffer_RemoveRange(buffer, at.y, buffer->count, encoder);
    }

    RemountTokensBasedOn(bufferView, at.x, 2);
    buffer = BufferView_GetBufferAt(bufferView, at.x);

    AllocatorFree(lineHelper);

    // get the buffer again in case it was end of a file
    bufferp1 = BufferView_GetBufferAt(bufferView, at.x+1);

    Buffer_Claim(buffer);
    Buffer_Claim(bufferp1);
    at.x += 1;
    at.y = Buffer_Utf8RawPositionToPosition(bufferp1, len, encoder);
    return at;
}

void AppCommandNewLine(){
    BufferView *bufferView = AppGetActiveBufferView();
    if(!bufferView->lineBuffer) return;
    NullRet(LineBuffer_IsWrittable(bufferView->lineBuffer));
    BufferView_SetRangeVisible(bufferView, 0);
    AppOnTypeChange();

    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);

    NullRet(buffer);

    UndoRedoUndoPushMerge(&bufferView->lineBuffer->undoRedo, buffer, cursor);

    cursor = AppCommandNewLine(bufferView, cursor);

    BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
    BufferView_AdjustGhostCursorIfOut(bufferView);
    LineBuffer_SetActiveBuffer(bufferView->lineBuffer, vec2i(cursor.x-1, OPERATION_INSERT_LINE));
    BufferView_Dirty(bufferView);
}

//TODO
void AppCommandRedo(){

}

void AppCommandKillEndOfLine(){
    BufferView *bView = AppGetActiveBufferView();
    NullRet(bView);
    NullRet(bView->lineBuffer);
    NullRet(LineBuffer_IsWrittable(bView->lineBuffer));
    AppOnTypeChange();

    vec2ui cursor = BufferView_GetCursorPosition(bView);
    EncoderDecoder *encoder = LineBuffer_GetEncoderDecoder(bView->lineBuffer);
    Buffer *buffer = BufferView_GetBufferAt(bView, cursor.x);
    NullRet(buffer);

    if(buffer->taken > 0){
        uint p = Buffer_Utf8RawPositionToPosition(buffer, cursor.y, encoder);
        UndoRedoUndoPushInsert(&bView->lineBuffer->undoRedo, buffer, cursor);

        Buffer_RemoveRangeRaw(buffer, p, buffer->taken, encoder);
        Buffer_Claim(buffer);
    }else{
        uint oc = cursor.x;
        AppDefaultRemoveOne(); // re-use erase 
        vec2ui after = BufferView_GetCursorPosition(bView);
        if(oc != after.x && oc < bView->lineBuffer->lineCount){
            BufferView_CursorToPosition(bView, oc, 0);
            BufferView_AdjustGhostCursorIfOut(bView);
        }
    }

    RemountTokensBasedOn(bView, cursor.x-1);
}

void AppCommandKillBuffer(){
    View *view = AppGetActiveView();
    BufferView *bufferView = AppGetActiveBufferView();
    NullRet(bufferView);
    if(!bufferView->lineBuffer){
        // NOTE: Check for viewers
        PdfViewState *pdfView = BufferView_GetPdfView(bufferView);
        if(!pdfView)
            return;

        // TODO: Currently the bufferview calls close document if
        //       it has a viewer it kinda makes this call function useless
        //       in this case but it feels like it is all we need.
        BufferView_SwapBuffer(bufferView, nullptr, EmptyView);
        View_ReturnToState(view, View_FreeTyping);
    }else{
        NullRet(LineBuffer_IsWrittable(bufferView->lineBuffer));

        LineBuffer *lineBuffer = bufferView->lineBuffer;
        if(lineBuffer){
            ViewTreeIterator iterator;
            ViewTree_Begin(&iterator);

            Tokenizer *tokenizer = FileProvider_GetLineBufferTokenizer(lineBuffer);
            SymbolTable *symTable = tokenizer->symbolTable;
            for(uint i = 0; i < lineBuffer->lineCount; i++){
                Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, i);
                Buffer_EraseSymbols(buffer, symTable);
            }

            while(iterator.value){
                View *view = iterator.value->view;
                BufferView *bView = &view->bufferView;
                if(bView->lineBuffer == lineBuffer){
                    BufferView_SwapBuffer(bView, nullptr, EmptyView);
                }
                ViewTree_Next(&iterator);
            }

            char *ptr = lineBuffer->filePath;
            uint pSize = lineBuffer->filePathSize;
            FileProvider_Remove(ptr, pSize);
            BufferViewLocation_RemoveLineBuffer(lineBuffer);
            LineBuffer_Free(lineBuffer);
            AllocatorFree(lineBuffer);
        }
    }
}

void AppCommandKillView(){
    uint count = ViewTree_GetCount();
    // only kill view if there is at least 2
    if(count > 1){
        BufferView *bView = AppGetActiveBufferView();
        // we need to make sure the interface is not expanded otherwise
        // UI will get weird.
        ControlCommands_RestoreExpand();
        AppCommandKillBuffer();
        BufferViewLocation_RemoveView(bView);
         // TODO: Check if it is safe to erase the view in the vnode here
        ViewNode *vnode = ViewTree_DestroyCurrent(1);
        AppSetViewingGeometry(appContext.currentGeometry, appContext.currentLineHeight);
        appContext.activeView = vnode->view;
        AppSetActiveView(appContext.activeView);
    }
}

void AppCommandUndo(){
    BufferView *bufferView = AppGetActiveBufferView();
    if(!bufferView->lineBuffer) return;
    NullRet(LineBuffer_IsWrittable(bufferView->lineBuffer));
    BufferView_SetRangeVisible(bufferView, 0);
    AppOnTypeChange();

    LineBuffer *lineBuffer = bufferView->lineBuffer;
    BufferChange *bChange = UndoRedoGetNextUndo(&lineBuffer->undoRedo);
    Tokenizer *tokenizer = FileProvider_GetLineBufferTokenizer(lineBuffer);
    SymbolTable *symTable = tokenizer->symbolTable;
    if(bChange){
        vec2ui cursor = BufferView_GetCursorPosition(bufferView);
        //TODO: Update symbol table on commands that require it
        switch(bChange->change){
            case CHANGE_NEWLINE:{
                cursor = AppCommandNewLine(bufferView, bChange->bufferInfo);
                RemountTokensBasedOn(bufferView, cursor.x);
                UndoRedoPopUndo(&lineBuffer->undoRedo);
            } break;

            /* These cases rely on replacing the original line with a saved one */
            case CHANGE_MERGE:
            case CHANGE_REMOVE:
            case CHANGE_INSERT:{
                Buffer *b = bChange->buffer;
                TokenizerStateContext *context = &b->stateContext;

                uint fTrack = context->forwardTrack;
                uint bTrack = context->backTrack;

                Buffer *buffer = LineBuffer_ReplaceBufferAt(bufferView->lineBuffer, b,
                                                            bChange->bufferInfo.x);

                if(bChange->change == CHANGE_MERGE){
                    /* In case of a merge we need to also remove the following line */
                    Buffer *bp1 = BufferView_GetBufferAt(bufferView, bChange->bufferInfo.x+1);
                    Buffer_EraseSymbols(bp1, symTable);
                    LineBuffer_RemoveLineAt(bufferView->lineBuffer,
                                            bChange->bufferInfo.x+1);
                }

                context = &buffer->stateContext;
                fTrack = Max(fTrack, context->forwardTrack);
                bTrack = Max(bTrack, context->backTrack);
                Buffer_EraseSymbols(buffer, symTable);

                UndoRedoPopUndo(&lineBuffer->undoRedo);
                // give ownership of the buffer to the undo system
                // this reduces memory allocations/frees
                UndoSystemTakeBuffer(buffer);

                cursor.x = bChange->bufferInfo.x;
                cursor.y = bChange->bufferInfo.y;
                uint base = cursor.x > bTrack ? cursor.x - bTrack : 0;
                RemountTokensBasedOn(bufferView, base, fTrack);
                // restore the active buffer and let stack grow again
                LineBuffer_SetActiveBuffer(bufferView->lineBuffer, vec2i((int)cursor.x,-1));
            } break;

            case CHANGE_BLOCK_REMOVE:{
                vec2ui start = bChange->bufferInfo;
                vec2ui end   = bChange->bufferInfoEnd;
                cursor = start;

                //printf("Removing range [%u %u] x [%u %u]\n",
                //       start.x, start.y, end.x, end.y); 

                AppCommandRemoveTextBlock(bufferView, start, end);

                RemountTokensBasedOn(bufferView, cursor.x, 1);
                UndoRedoPopUndo(&lineBuffer->undoRedo);

            } break;
            case CHANGE_BLOCK_INSERT:{
                vec2ui start = bChange->bufferInfo;
                char *text = bChange->text;
                uint size = bChange->size;
                if(text && size > 0){
                    uint off = 0;
                    uint startBuffer = start.x;
                    uint n = LineBuffer_InsertRawTextAt(bufferView->lineBuffer, text, size,
                                                        start.x, start.y, &off);

                    uint endx = start.x + n;
                    uint endy = off;
                    LineBuffer_SetActiveBuffer(bufferView->lineBuffer, vec2i(endx, -1));
                    RemountTokensBasedOn(bufferView, startBuffer, n);
                    cursor.x = endx;
                    cursor.y = endy;
                    UndoRedoPopUndo(&lineBuffer->undoRedo);
                    AllocatorFree(text);
                }
            } break;
            default:{ printf("Unknow undo command\n"); }
        }

        BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
        BufferView_AdjustGhostCursorIfOut(bufferView);
    }
}

void AppCommandHomeLine(){
    BufferView *bufferView = AppGetActiveBufferView();
    if(!bufferView->lineBuffer) return;
    BufferView_SetRangeVisible(bufferView, 0);

    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
    EncoderDecoder *encoder = LineBuffer_GetEncoderDecoder(bufferView->lineBuffer);

    if(buffer == nullptr){
        cursor.y = 0;
    }else if(Buffer_IsBlank(buffer)){
        cursor.y = 0;
    }else{
        uint tid = Buffer_FindFirstNonEmptyToken(buffer);
        Token *token = &buffer->tokens[tid];
        uint p8 = Buffer_Utf8RawPositionToPosition(buffer, token->position, encoder);
        if(cursor.y <= p8) cursor.y = 0;
        else{
            cursor.y = p8;
        }
    }

    BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
}

void AppCommandQueryBarSearchAndReplace(){
    View *view = AppGetActiveView();
    ViewState state = View_GetState(view);
    NullRet(view->bufferView.lineBuffer);
    NullRet(LineBuffer_IsWrittable(view->bufferView.lineBuffer));
    AppOnTypeChange();
    if(state != View_QueryBar){
        QueryBar *bar = View_GetQueryBar(view);
        QueryBarCmdSearchAndReplace *replace = &bar->replaceCmd;
        replace->state = QUERY_BAR_SEARCH_AND_REPLACE_SEARCH;

        auto onConfirm = [&](QueryBar *qbar, View *vview, int accepted) -> int{
            // The search and replace command uses the search command under the hood
            // so you need to get that result too.
            QueryBarCmdSearch *searchResult = &qbar->searchCmd;
            QueryBarCmdSearchAndReplace *searchReplace = &qbar->replaceCmd;
            if(accepted){
                BufferView *bView = View_GetBufferView(vview);
                Buffer *buf = BufferView_GetBufferAt(bView, searchResult->lineNo);
                if(buf){
                    Tokenizer *tokenizer = FileProvider_GetLineBufferTokenizer(bView->lineBuffer);
                    EncoderDecoder *encoder = LineBuffer_GetEncoderDecoder(bView->lineBuffer);
                    SymbolTable *symTable = tokenizer->symbolTable;
                    vec2ui cursor = BufferView_GetCursorPosition(bView);

                    UndoRedoUndoPushInsert(&bView->lineBuffer->undoRedo, buf, cursor);

                    Buffer_EraseSymbols(buf, symTable);

                    Buffer_RemoveRangeRaw(buf, searchResult->position,
                                searchResult->position + searchReplace->toLocateLen, encoder);
                    if(searchReplace->toReplaceLen > 0){
                        Buffer_InsertRawStringAt(buf, searchResult->position,
                                searchReplace->toReplace, searchReplace->toReplaceLen, encoder);
                    }
                    RemountTokensBasedOn(bView, cursor.x);
                    BufferView_Dirty(bView);
                }
            }
            return 0;
        };

        QueryBar_SetInteractiveSearchCallback(bar, onConfirm);
        QueryBar_Activate(bar, QUERY_BAR_CMD_SEARCH_AND_REPLACE, view);
        View_ReturnToState(view, View_QueryBar);
        KeyboardSetActiveMapping(appContext.queryBarMapping);
    }
}

bool AppIsPathFromRoot(std::string path){
    std::string rootDir = AppGetRootDirectory();
    return StringStartsWith((char *)path.c_str(), path.size(),
                            (char *)rootDir.c_str(), rootDir.size());
}

void AppCommandQueryBarInteractiveCommand(){
    View *gView = AppGetActiveView();
    ViewState state = View_GetState(gView);
    if(state != View_QueryBar){
        // NOTE: This needs to be static otherwise if operation is detached
        //       we lose the value
        static int oldCompression = AppGetPathCompression();
        AppSetPathCompression(-1);
        QueryBar *qbar = View_GetQueryBar(gView);

        auto emptyFunc = [&](QueryBar *bar, View *view) -> int{ return 0; };
        auto cancelFunc = [&](QueryBar *bar, View *view) -> int{
            AppSetPathCompression(oldCompression);
            return 0;
        };
        auto onCommit = [&](QueryBar *bar, View *view) -> int{
            char *content = nullptr;
            uint size = 0;
            AppOnTypeChange();
            QueryBar_GetWrittenContent(bar, &content, &size);
            int r = BaseCommand_Interpret(content, size, view);
            AppSetPathCompression(oldCompression);
            return r == 2 ? 0 : -1;
        };

        QueryBar_ActiveCustomFull(qbar, nullptr, 0, emptyFunc,
                                  cancelFunc, onCommit, nullptr,
                                  QUERY_BAR_CMD_INTERACTIVE);

        View_ReturnToState(gView, View_QueryBar);
        KeyboardSetActiveMapping(appContext.queryBarMapping);
        QueryBar_EnableCursorJump(qbar);
    }
}

void AppCommandStoreFileCommand(){
    View *view = AppGetActiveView();
    NullRet(view->bufferView.lineBuffer);
    NullRet(LineBuffer_IsWrittable(view->bufferView.lineBuffer));
    std::string writtenPath = std::string(view->bufferView.lineBuffer->filePath);
    std::string rootDir = AppGetRootDirectory();
    if(AppIsPathFromRoot(writtenPath)){
        writtenPath = std::string(&view->bufferView.lineBuffer->filePath[rootDir.size()+1]);
    }else{
        printf("Warning: linebuffer has inconsistent path\n");
        printf("File Path : %s\n", view->bufferView.lineBuffer->filePath);
        return;
    }

    int r = BaseCommand_AddFileEntryIntoAutoLoader((char *)writtenPath.c_str(),
                                                   writtenPath.size());
    if(r == 0){
        AppAddStoredFile(writtenPath);
    }
}

void AppCommandQueryBarSearch(){
    View *view = AppGetActiveView();
    ViewState state = View_GetState(view);
    NullRet(view->bufferView.lineBuffer);
    if(state != View_QueryBar){
        QueryBar *bar = View_GetQueryBar(view);
        QueryBar_Activate(bar, QUERY_BAR_CMD_SEARCH, view);
        View_ReturnToState(view, View_QueryBar);

        KeyboardSetActiveMapping(appContext.queryBarMapping);
    }
}

void AppCommandQueryBarGotoLine(){
    View *view = AppGetActiveView();
    ViewState state = View_GetState(view);
    NullRet(view->bufferView.lineBuffer);
    if(state != View_QueryBar){
        QueryBar *bar = View_GetQueryBar(view);
        QueryBar_Activate(bar, QUERY_BAR_CMD_JUMP_TO_LINE, view);
        View_ReturnToState(view, View_QueryBar);

        KeyboardSetActiveMapping(appContext.queryBarMapping);
    }
}

void AppCommandQueryBarRevSearch(){
    View *view = AppGetActiveView();
    ViewState state = View_GetState(view);
    NullRet(view->bufferView.lineBuffer);
    if(state != View_QueryBar){
        QueryBar *bar = View_GetQueryBar(view);
        QueryBar_Activate(bar, QUERY_BAR_CMD_REVERSE_SEARCH, view);
        View_ReturnToState(view, View_QueryBar);

        KeyboardSetActiveMapping(appContext.queryBarMapping);
    }
}

void AppCommandEndLine(){
    BufferView *bufferView = AppGetActiveBufferView();
    NullRet(bufferView->lineBuffer);
    BufferView_SetRangeVisible(bufferView, 0);

    vec2ui cursor = BufferView_GetCursorPosition(bufferView);
    Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);
    cursor.y = buffer->count;
    BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
}

ViewNode *AppGetNextViewNode(){
    int n = 0;
    View *view = appContext.activeView;
    ViewTreeIterator iterator;
    ViewTree_Begin(&iterator);

    ViewNode *vnode = iterator.value;
    while(iterator.value){
        if(n == 1){
            vnode = iterator.value;
            break;
        }

        if(view == iterator.value->view){
            n = 1;
        }

        ViewTree_Next(&iterator);
    }

    return vnode;
}

void AppCommandSwapView(){
    NullRet(!ControlCommands_IsExpanded());
    ViewNode *vnode = AppGetNextViewNode();
    if(vnode->view){
        if(BufferView_IsVisible(&vnode->view->bufferView)){
            ViewTree_SetActive(vnode);
            AppSetActiveView(vnode->view);
        }
    }
}

void AppCommandSetGhostCursor(){
    BufferView *bufferView = AppGetActiveBufferView();
    NullRet(bufferView->lineBuffer);
    BufferView_GhostCursorFollow(bufferView);
    BufferView_SetRangeVisible(bufferView, 0);
}

void AppCommandSwapLineNbs(){
    BufferView *bufferView = AppGetActiveBufferView();
    NullRet(bufferView->lineBuffer);
    BufferView_ToogleLineNumbers(bufferView);
}

void AppCommandAutoComplete(){
    View *view = AppGetActiveView();
    BufferView *bView = AppGetActiveBufferView();
    NullRet(bView);
    NullRet(bView->lineBuffer);
    NullRet(LineBuffer_IsWrittable(bView->lineBuffer));
    AppOnTypeChange();

    vec2ui cursor = BufferView_GetCursorPosition(bView);
    Buffer *buffer = BufferView_GetBufferAt(bView, cursor.x);
    EncoderDecoder *encoder = LineBuffer_GetEncoderDecoder(bView->lineBuffer);
    NullRet(buffer);

    //TODO: Comments cannot find their token id
    uint tid = Buffer_GetTokenAt(buffer, cursor.y > 0 ? cursor.y - 1 : 0, encoder);
    if(buffer->tokenCount > 0){
        if(tid <= buffer->tokenCount-1){
            Token *token = &buffer->tokens[tid];
            if(token->identifier != TOKEN_ID_SPACE){
                SelectableList *list = view->autoCompleteList;
                char *ptr = &buffer->data[token->position];
                if(View_GetState(view) != View_AutoComplete){
                    appGlobalConfig.autoCompleteSize = (int)token->size;
                }

                int n = AutoComplete_Search(ptr, token->size, list);
                if(n > 0 && View_GetState(view) != View_AutoComplete){
                    View_ReturnToState(view, View_AutoComplete);
                    AppSetBindingsForState(View_AutoComplete);
                    SelectableList_ResetView(list);
                    if(n == 1){
                        AutoComplete_Commit();
                        appGlobalConfig.autoCompleteSize = 0;
                    }
                }
            }
        }
    }
}

void AppCommandLineQuicklyDisplay(){
    BufferView *bufferView = AppGetActiveBufferView();
    NullRet(bufferView->lineBuffer);
    BufferView_StartNumbersShowTransition(bufferView, 3.0);
}

void AppCommandSaveBufferView(){
    BufferView *bufferView = AppGetActiveBufferView();
    bool success = false;
    NullRet(bufferView->lineBuffer);
    if(LineBuffer_IsEncrypted(bufferView->lineBuffer)){
        success = LineBuffer_SaveToStorageEncrypted(bufferView->lineBuffer);
    }else{
        success = LineBuffer_SaveToStorage(bufferView->lineBuffer);
    }

    bufferView->lineBuffer->is_dirty = !success;
}

void AppCommandCopy(){
    vec2ui start, end;
    char *ptr = nullptr;
    BufferView *view = AppGetActiveBufferView();
    NullRet(view->lineBuffer);

    if(BufferView_GetCursorSelectionRange(view, &start, &end)){
        uint size = LineBuffer_GetTextFromRange(view->lineBuffer, &ptr, start, end);
        ClipboardSetString(ptr, size);
    }
}

void AppCommandJumpNesting(){
    DoubleCursor *cursor = nullptr;
    BufferView *view = AppGetActiveBufferView();
    NullRet(view->lineBuffer);
    EncoderDecoder *encoder = LineBuffer_GetEncoderDecoder(view->lineBuffer);
    BufferView_GetCursor(view, &cursor);

    if(BufferView_CursorNestIsValid(view)){
        //TODO: Create toogle
        if(view->activeNestPoint == -1){
            view->activeNestPoint = 0;
        }else{
            view->activeNestPoint = 1 - view->activeNestPoint;
        }

        vec2ui p = view->activeNestPoint == 0 ? cursor->nestStart : cursor->nestEnd;
        Buffer *buffer = BufferView_GetBufferAt(view, p.x);
        uint pos = Buffer_Utf8RawPositionToPosition(buffer, buffer->tokens[p.y].position, encoder);
        BufferView_CursorToPosition(view, p.x, pos);
        BufferView_SetRangeVisible(view, 0);
    }
}

void AppCommandCut(){
    AppHandleRegionCut();
}

void AppPasteString(const char *p, uint size, bool force_view){
    View *vview = AppGetActiveView();
    ViewState state = View_GetState(vview);
    if(state == View_FreeTyping || state == View_AutoComplete || force_view){
        BufferView *view = AppGetActiveBufferView();
        NullRet(view->lineBuffer);
        NullRet(LineBuffer_IsWrittable(view->lineBuffer));
        AppOnTypeChange();

        Tokenizer *tokenizer = FileProvider_GetLineBufferTokenizer(view->lineBuffer);
        SymbolTable *symTable = tokenizer->symbolTable;
        BufferView_SetRangeVisible(view, 0);

        if(size > 0 && p){
            CopySection section;
            Buffer *buffer = nullptr;
            uint off = 0;
            vec2ui cursor = BufferView_GetCursorPosition(view);
            uint startBuffer = cursor.x;

            buffer = BufferView_GetBufferAt(view, cursor.x);

            Buffer_EraseSymbols(buffer, symTable);

            uint n = LineBuffer_InsertRawTextAt(view->lineBuffer, (char *) p, size,
                                                cursor.x, cursor.y, &off);

            section.start = cursor;
            section.end = vec2ui(cursor.x + n, off);
            section.currTime = 0;
            section.interval = kTransitionCopyFadeIn;
            section.active = 1;

            uint endx = cursor.x + n;
            uint endy = off;

            UndoRedoUndoPushRemoveBlock(&view->lineBuffer->undoRedo,
            cursor, vec2ui(endx, endy));
            LineBuffer_SetActiveBuffer(view->lineBuffer, vec2i(endx, -1), 0);
            LineBuffer_SetCopySection(view->lineBuffer, section);

            RemountTokensBasedOn(view, startBuffer, n);

            Buffer *b = LineBuffer_GetBufferAt(view->lineBuffer, endx);

            if(b == nullptr){
                BUG();
                BUG_PRINT("Cursor line is nullptr\n");
            }

            cursor.x = endx;
            cursor.y = Clamp(endy, (uint)0, b->count);
            BufferView_CursorToPosition(view, cursor.x, cursor.y);
            BufferView_Dirty(view);
        }
    }else if(View_IsQueryBarActive(vview)){
        QueryBar *bar = View_GetQueryBar(vview);
        int r = QueryBar_AddEntry(bar, vview, (char *)p, size);
        if(r == 1){
            AppQueryBarSearchJumpToResult(bar, vview);
        }else if(r == -2){
            AppDefaultReturn();
        }
    }
}

void AppCommandPaste(){
    uint size = 0;
    const char *p = ClipboardGetString(&size);
    AppPasteString(p, size);
}

uint AppComputeLineIndentLevel(Buffer *buffer, uint p, EncoderDecoder *encoder){
    TokenizerStateContext *ctx = &buffer->stateContext;
    uint tid = Min(Buffer_GetTokenAt(buffer, p, encoder), buffer->tokenCount);
    uint l = ctx->indentLevel;
    for(uint i = 0; i < tid; i++){
        Token *token = &buffer->tokens[i];
        if(token->identifier == TOKEN_ID_BRACE_CLOSE){
            l = l > 0 ? l - 1 : 0;
        }else if(token->identifier == TOKEN_ID_BRACE_OPEN){
            l++;
        }
    }
    return l;
}

uint AppComputeLineLastIndentLevel(Buffer *buffer){
    TokenizerStateContext *ctx = &buffer->stateContext;
    uint l = ctx->indentLevel;
    for(uint i = 0; i < buffer->tokenCount; i++){
        Token *token = &buffer->tokens[i];
        if(token->identifier == TOKEN_ID_BRACE_CLOSE){
            l = l > 0 ? l - 1 : 0;
        }else if(token->identifier == TOKEN_ID_BRACE_OPEN){
            l++;
        }
    }

    return l;
}

uint AppComputeLineIndentLevel(Buffer *buffer){
    TokenizerStateContext *ctx = &buffer->stateContext;
    uint l = ctx->indentLevel;
    for(uint i = 0; i < buffer->tokenCount; i++){
        Token *token = &buffer->tokens[i];
        if(token->identifier != TOKEN_ID_SPACE){
            if(token->identifier == TOKEN_ID_BRACE_CLOSE){
                l = l > 0 ? l - 1 : 0;
            }

            break;
        }
    }

    return l;
}

void AppCommandIndentRegion(BufferView *view, vec2ui start, vec2ui end){
    char *lineHelper = nullptr;
    EncoderDecoder *encoder = LineBuffer_GetEncoderDecoder(view->lineBuffer);
    uint tabSize = appGlobalConfig.useTabs ? 1 : appGlobalConfig.tabLength;
    char indentChar = ' ';
    int changes = 0;
    if(appGlobalConfig.useTabs){
        indentChar = '\t';
    }

    // Grab the maximum line size in this range
    uint maxSize = 0;
    for(uint i = start.x; i < end.x; i++){
        Buffer *buffer = BufferView_GetBufferAt(view, i);
        maxSize = Max(buffer->taken, maxSize);
    }

    lineHelper = AllocatorGetN(char, maxSize);
    for(uint i = start.x; i < end.x; i++){
        Buffer *buffer = BufferView_GetBufferAt(view, i);
        TokenizerStateContext *ctx = &buffer->stateContext;
        // 1- Remove all empty tokens at start of the buffer,
        //    this is usually 1 because our lex groups spaces
        //    but just in case loop this thing.
        uint targetP = 0;
        uint foundNonSpace = 0;
        int indentStyle = 0;
        uint indentLevel = ctx->indentLevel;
        for(uint k = 0; k < buffer->tokenCount; k++){
            Token *token = &buffer->tokens[k];
            if(token->identifier != TOKEN_ID_SPACE){
                targetP = token->position;
                foundNonSpace = 1;
                //TODO: Is this sufficient to detect ending of a nest level?
                if(token->identifier == TOKEN_ID_BRACE_CLOSE){
                    indentLevel = indentLevel > 0 ? indentLevel - 1 : 0;
                }else if(token->identifier == TOKEN_ID_PREPROCESSOR){
                    indentStyle = 1;
                }else if(token->identifier == TOKEN_ID_COMMENT){
                    indentStyle = 2;
                }
                break;
            }
        }

        // 2- Add the indentation sequence the amount of times required
        uint len = indentLevel * tabSize;
        uint llen = buffer->taken - targetP;

        // Skip lines made entirely of spaces
        if(!foundNonSpace){
            llen = 0;
            len = 0;
        }

        // if this line is a preprocessor align to edge of file
        if(indentStyle == 1){
            len = 0;
        }

        // don't attempt to configure user comment format
        if(indentStyle == 2){
            // however if we don't allow indentation until first
            // token we would keep the tab/space as was defined
            len = targetP;
        }

        if(len+llen > maxSize){
            uint osize = maxSize;
            maxSize = len + llen;
            lineHelper = AllocatorExpand(char, lineHelper, maxSize, osize);
        }

        // insert spacing only when there is actually a token
        if(len > 0 && foundNonSpace)
            Memset(lineHelper, indentChar, len * sizeof(char));
        // insert rest of the line
        if(llen > 0)
            Memcpy(&lineHelper[len], &buffer->data[targetP], llen * sizeof(char));

        //TODO: Maybe we can batch all lines as a CHANGE_BLOCK_INSERT for simpler undo,
        // this undo is currently broken and I'm not sure why.
        // Save undo - redo stuff as simple inserts
        //UndoRedoUndoPushInsert(&view->lineBuffer->undoRedo, buffer, vec2ui(i, 0));

        // Re-insert the line in raw mode to avoid processing
        Buffer_RemoveRange(buffer, 0, buffer->count, encoder);
        if(len + llen > 0)
            Buffer_InsertRawStringAt(buffer, 0, lineHelper, len+llen, encoder);
        changes = 1;
    }

    if(changes){ // unfortunatelly we need to retokenize to adjust tokens offsets
        RemountTokensBasedOn(view, start.x, end.x - start.x);
    }

    AllocatorFree(lineHelper);
}

void AppCommandIndentCurrent(){
    BufferView *view = AppGetActiveBufferView();
    NullRet(view->lineBuffer);
    NullRet(LineBuffer_IsWrittable(view->lineBuffer));
    if(view->lineBuffer->lineCount > 0){
        AppCommandIndentRegion(view, vec2ui(0,0), vec2ui(view->lineBuffer->lineCount, 0));
        BufferView_Dirty(view);
    }
}

void AppCommandIndent(){
    vec2ui start, end;
    BufferView *view = AppGetActiveBufferView();
    NullRet(view->lineBuffer);
    NullRet(LineBuffer_IsWrittable(view->lineBuffer));
    if(BufferView_GetCursorSelectionRange(view, &start, &end)){
        AppCommandIndentRegion(view, start, end);
        BufferView_Dirty(view);
    }
}

void AppCommandSplitHorizontal(){
    ViewTree_SplitHorizontal();
    AppSetViewingGeometry(appContext.currentGeometry, appContext.currentLineHeight);
    View_SetActive(appContext.activeView, 1);
}

void AppCommandSplitVertical(){
    ViewTree_SplitVertical();
    AppSetViewingGeometry(appContext.currentGeometry, appContext.currentLineHeight);
    View_SetActive(appContext.activeView, 1);
}

void AppCommandListFunctions(){
    (void)BaseCommand_SearchFunctions((char *)CMD_FUNCTIONS_STR, strlen(CMD_FUNCTIONS_STR),
                                      AppGetActiveView());
}

void AppDefaultEntry(char *utf8Data, int utf8Size, void *){
    if(utf8Size > 0){
        int off = 0;
        EncoderDecoder *encoder = nullptr;
        View *view = AppGetActiveView();
        ViewState state = View_GetState(view);
        if(state == View_FreeTyping || state == View_AutoComplete){
            BufferView *bufferView = AppGetActiveBufferView();
            NullRet(bufferView);
            NullRet(bufferView->lineBuffer);
            encoder = LineBuffer_GetEncoderDecoder(bufferView->lineBuffer);
        }else if(View_IsQueryBarActive(view)){
            QueryBar *bar = View_GetQueryBar(view);
            encoder = &bar->encoder;
        }

        /*
        * NOTE: Input system (keyboard/X/Win32) always produce UTF-8 strings
        */
        ConvertFromUTF8(encoder, utf8Data, &utf8Size);

        int cp = StringToCodepoint(encoder, utf8Data, utf8Size, &off);
        if(Font_SupportsCodepoint(cp)){
            if(state == View_FreeTyping || state == View_AutoComplete){
                BufferView *bufferView = AppGetActiveBufferView();
                NullRet(LineBuffer_IsWrittable(bufferView->lineBuffer));
                AppOnTypeChange();
                BufferView_SetRangeVisible(bufferView, 0);

                Tokenizer *tokenizer = FileProvider_GetLineBufferTokenizer(bufferView->lineBuffer);
                SymbolTable *symTable = tokenizer->symbolTable;

                vec2ui cursor = BufferView_GetCursorPosition(bufferView);
                Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor.x);

                NullRet(buffer);

                vec2i id = LineBuffer_GetActiveBuffer(bufferView->lineBuffer);
                if(id.x != (int)cursor.x || id.y != OPERATION_INSERT_CHAR){
                    // user started to edit a different buffer
                    UndoRedoUndoPushRemove(&bufferView->lineBuffer->undoRedo,
                                           buffer, cursor);

                    LineBuffer_SetActiveBuffer(bufferView->lineBuffer,
                                               vec2i((int)cursor.x, OPERATION_INSERT_CHAR));
                }

                //TODO: In case this thing is a lexical context, i.e.: '{', '}', ...
                //      we need to go further to update the logical identation level

                uint startX = cursor.x;
                uint offset = 0;

                Buffer_EraseSymbols(buffer, symTable);
                Buffer_InsertStringAt(buffer, cursor.y, utf8Data, utf8Size, encoder);

                if(utf8Size == 1){ // TODO: make this better
                    if(*utf8Data == '}' || *utf8Data == '{'){
                        if(BufferView_CursorNestIsValid(bufferView)){
                            NestPoint start = bufferView->startNest[0];
                            NestPoint end   = bufferView->endNest[start.comp];
                            startX = start.position.x;
                            uint endX = end.position.x;
                            offset = endX - startX;
                        }
                    }
                }

                Buffer_Claim(buffer);
                RemountTokensBasedOn(bufferView, startX, offset);

                cursor.y += StringComputeU8Count(encoder, utf8Data, utf8Size);

                BufferView_CursorToPosition(bufferView, cursor.x, cursor.y);
                BufferView_Dirty(bufferView);

                if(state == View_AutoComplete){
                    if(utf8Size == 1 && AUTOCOMPLETE_TERMINATOR(*utf8Data)){
                        AutoComplete_Interrupt();
                    }else{
                        AppCommandAutoComplete();
                    }
                }

            }else if(View_IsQueryBarActive(view)){
                QueryBar *bar = View_GetQueryBar(view);
                int r = QueryBar_AddEntry(bar, view, utf8Data, utf8Size);
                if(r == 1){
                    AppQueryBarSearchJumpToResult(bar, view);
                }else if(r == -2){
                    AppDefaultReturn();
                }
            }
        }
    }
}

View *AppGetViewAt(int x, int y){
    ViewTreeIterator iterator;
    View *rView = nullptr, *view = nullptr;
    vec2ui r(x, y);
    ViewTree_Begin(&iterator);
    while(iterator.value){
        view = iterator.value->view;
        if(Geometry_IsPointInside(&view->geometry, r)){
            rView = view;
            break;
        }

        ViewTree_Next(&iterator);
    }

    return rView;
}

vec2ui AppComputeViewMouseCoordinates(int x, int y){
    vec2ui r(x, y);
    View *view = nullptr;
    ViewTreeIterator iterator;

    ViewTree_Begin(&iterator);
    while(iterator.value){
        view = iterator.value->view;
        if(Geometry_IsPointInside(&view->geometry, r)){
            // Activate the view and re-map position
            ViewTree_SetActive(iterator.value);
            r = r - view->geometry.lower;
            break;
        }

        ViewTree_Next(&iterator);
    }

    return r;
}

vec2ui AppActivateViewAt(int x, int y, bool force_binding){
    // Check which view is the mouse on and activate it
    vec2ui r(x, y);
    View *view = nullptr;
    ViewTreeIterator iterator;

    ViewTree_Begin(&iterator);
    while(iterator.value){
        view = iterator.value->view;
        if(Geometry_IsPointInside(&view->geometry, r)){
            // Activate the view and re-map position
            ViewTree_SetActive(iterator.value);

            // we can do better than this, fix it
            AppSetActiveView(view, force_binding);
            r = r - view->geometry.lower;
            break;
        }

        ViewTree_Next(&iterator);
    }

    return r;
}

void AppCommandEnterKey(){
    AppOnTypeChange();
    BufferView *bufferView = AppGetActiveBufferView();
    ViewType type = BufferView_GetViewType(bufferView);
    switch(type){
        case CodeView: AppCommandNewLine(); break;
        case ImageView: break;
        //case GitStatusView: AppCommandGitStatusChangeBuffer(); break;
        default: {
            printf("Unimplemented enter for view type %s\n", ViewTypeString(type));
        }
    }
}

void AppCommandSwapToBuildBuffer(){
    View *view = AppGetActiveView();
    LockedLineBuffer *lockedBuffer = nullptr;
    GetExecutorLockedLineBuffer(&lockedBuffer);
    ViewNode *vnode = AppGetNextViewNode();
    BufferView *bView = View_GetBufferView(view);
    if(vnode){
        if(vnode->view){
            bView = View_GetBufferView(vnode->view);
        }
    }

    BufferView_SwapBuffer(bView, lockedBuffer->lineBuffer, CodeView);
}

void AppCommandQueryBarLeftArrow(){
    View *view = AppGetActiveView();
    QueryBar *bar = View_GetQueryBar(view);
    QueryBar_MoveLeft(bar);
}

void AppCommandQueryBarRightArrow(){
    View *view = AppGetActiveView();
    QueryBar *bar = View_GetQueryBar(view);
    QueryBar_MoveRight(bar);
}

void AppCommandQueryBarJumpNext(){
    View *view = AppGetActiveView();
    QueryBar *bar = View_GetQueryBar(view);
    QueryBar_JumpToNext(bar);
}

void AppCommandQueryBarJumpPrevious(){
    View *view = AppGetActiveView();
    QueryBar *bar = View_GetQueryBar(view);
    QueryBar_JumpToPrevious(bar);
}

void AppCommandQueryBarNext(){
    View *view = AppGetActiveView();
    QueryBar *bar = View_GetQueryBar(view);
    if(View_NextItem(view) != 0){
        AppQueryBarSearchJumpToResult(bar, view);
    }
}

void AppCommandQueryBarPrevious(){
    View *view = AppGetActiveView();
    QueryBar *bar = View_GetQueryBar(view);
    if(View_PreviousItem(view) != 0){
        AppQueryBarSearchJumpToResult(bar, view);
    }
}

void AppDefaultReturn(){
    View *view = AppGetActiveView();
    ViewState state = View_GetDefaultState(view);
    if(View_GetState(view) != state){
        BufferView *bView = View_GetBufferView(view);
        ViewType type = BufferView_GetViewType(bView);
        View_ReturnToState(view, state);
        AppSetBindingsForState(state, type);
    }
}

void AppCommandQueryBarCommit(){
    View *view = AppGetActiveView();
    ViewState state = View_GetDefaultState(view);
    if(View_GetState(view) != state){
        if(View_CommitToState(view, state) != 0){
            BufferView *bView = View_GetBufferView(view);
            ViewType type = BufferView_GetViewType(bView);
            AppSetBindingsForState(state, type);
        }
    }
}

void AppCommandSwitchFont(){
    View *view = AppGetActiveView();
    int r = SwitchFontCommandStart(view);
    if(r >= 0)
        AppSetBindingsForState(View_SelectableList);
}

void AppCommandSwitchTheme(){
    View *view = AppGetActiveView();
    int r = SwitchThemeCommandStart(view);
    if(r >= 0)
        AppSetBindingsForState(View_SelectableList);
}

void AppCommandSwitchBuffer(){
    AppRestoreCurrentBufferViewState();
    View *view = AppGetActiveView();
    int r = SwitchBufferCommandStart(view);
    if(r >= 0)
        AppSetBindingsForState(View_SelectableList);
}

void AppDragAndDrop(char **paths, int count, int x, int y, void*){
    if(count == 0)
        return;

    char folder[PATH_MAX];
    StorageDevice *storage = FetchStorageDevice();
    if(!storage->IsLocallyStored()) // don't do drag and drop on remote filesystems
        return;

    bool insert_into_view = true;
    OpenGLState *state = Graphics_GetGlobalContext();

    View *view = AppGetViewAt(x, state->height - y);
    ViewState vstate = View_GetState(view);
    insert_into_view = vstate == View_FreeTyping;

    AppSetActiveView(view, true);

    for(int i = 0; i < count; i++){
        FileEntry entry;
        std::string value(paths[i]);
        SwapPathDelimiter(value);
        uint len = value.size();
        char *valuec = (char *)value.c_str();
        int r = GuessFileEntry(valuec, len, &entry, folder);
        int fileType = -1;
        if(!(r < 0) && entry.type == DescriptorFile){
            if(!FileProvider_IsFileLoaded(valuec, len)){
                LineBuffer *lineBuffer = nullptr;
                // NOTE: We can't async here as there might be multiple files being dropped
                //       I'm not sure the lexer can handle parallel requests under the same
                //       tokenizer
                r = FileProvider_Load((char *)valuec, len, fileType, &lineBuffer, false);
                if(r == FILE_LOAD_REQUIRES_DECRYPT) // TODO: Should we do something here?
                    FileProvider_ReleaseTemporary();
                else if(r == FILE_LOAD_SUCCESS && i == 0 && insert_into_view)
                    BufferView_SwapBuffer(View_GetBufferView(view), lineBuffer, CodeView);
            }
        }
    }
}

void AppCommandOpenFileWithViewType(ViewType type, int creationFlags){
    AppRestoreCurrentBufferViewState();
    int localFlags = creationFlags;
    View *view = AppGetActiveView();

    auto emptyFunc = [&](QueryBar *bar, View *view) -> int{ return 0; };
    auto fileOpenEncrypted = [&](QueryBar *bar, View *view) -> int{
        BufferView *bView = View_GetBufferView(view);
        Tokenizer *tokenizer = nullptr;
        LineBuffer *lBuffer = nullptr;
        char *content = nullptr;
        uint size = 0;
        int fileType = -1;
        AppOnTypeChange();
        QueryBar_GetWrittenContent(bar, &content, &size);

        if(FileProvider_LoadTemporary(content, size, fileType, &lBuffer, false) ==
           FILE_LOAD_SUCCESS)
        {
            BufferView_SwapBuffer(bView, lBuffer, CodeView);
        }

        return 1;
    };

    auto fileOpen = [=](View *view, FileEntry *entry) -> void{
        char targetPath[PATH_MAX];
        Tokenizer *tokenizer = nullptr;
        LineBuffer *lBuffer = nullptr;
        BufferView *bView = View_GetBufferView(view);
        if(entry){
            FileOpener *opener = View_GetFileOpener(view);
            uint l = snprintf(targetPath, PATH_MAX, "%s%s", opener->basePath, entry->path);
            if(entry->isLoaded == 0){
                int fileType = -1;
                int ret = FileProvider_Load(targetPath, l, fileType, &lBuffer, false);
                if(ret == FILE_LOAD_SUCCESS)
                    BufferView_SwapBuffer(bView, lBuffer, CodeView);
                else if(ret == FILE_LOAD_REQUIRES_DECRYPT){
                    QueryBarInputFilter filter = INPUT_FILTER_INITIALIZER;
                    filter.toHistory = false;
                    QueryBar *qbar = View_GetQueryBar(view);
                    QueryBar_SetPendingCall(qbar, (char *)"Password", 8, emptyFunc,
                                            emptyFunc, fileOpenEncrypted, &filter);
                }else if(ret == FILE_LOAD_REQUIRES_VIEWER){
                    uint pdfLen = 0;
                    char *pdf = FileProvider_GetTemporary(&pdfLen);
                    if(pdf && pdfLen > 0){
                        int _width, _height;
                        // TODO: Parse pdf, get image ptr, and setup whatever we need
                        //       keyboard, rendering, etc...
                        PdfViewState *pdfView = nullptr;
                        if(!PdfView_OpenDocument(&pdfView, pdf, pdfLen, targetPath, l)){
                            return;
                        }

                        BufferView_SwapBuffer(bView, nullptr, ImageView);
                        BufferView_SetPdfView(bView, pdfView);
                        PdfView_OpenPage(pdfView, 0);
                        unsigned char *img = (unsigned char *)
                                        PdfView_GetCurrentPage(pdfView, _width, _height);
                        BufferView_ResetImageRenderer(bView, _width, _height, img);
                    }
                }
            }else{
                int f = FileProvider_FindByPath(&lBuffer, targetPath, l, &tokenizer);
                if(f == 0 || lBuffer == nullptr){
                    printf("Did not find buffer %s\n", entry->path);
                }else{
                    //printf("Swapped to %s\n", entry->path);
                    BufferView_SwapBuffer(bView, lBuffer, CodeView);
                }
            }
        }else if((localFlags & OPEN_FILE_FLAGS_ALLOW_CREATION) && App_IsStateWrite()){
            char *content = nullptr;
            uint len = 0;
            QueryBar *querybar = View_GetQueryBar(view);
            QueryBar_GetWrittenContent(querybar, &content, &len);
            if(len > 0){
                //printf("Creating file %s\n", content);
                FileProvider_CreateFile(content, len, &lBuffer, &tokenizer);
                BufferView_SwapBuffer(bView, lBuffer, type);
            }
        }
        AppSetDelayedCall(AppCommandQueryBarCommit);
    };

    int r = FileOpenerCommandStart(view, appContext.cwd,
                                   strlen(appContext.cwd), fileOpen);
    if(r >= 0){
        AppSetBindingsForState(View_SelectableList);
    }
}

void AppCommandOpenFile(){
    AppCommandOpenFileWithViewType(CodeView, OPEN_FILE_FLAGS_ALLOW_CREATION);
}

void DbgSupport_ResetBreakpointMap(){
    appContext.dbgBreaks.clear();
}

void DbgSupport_ResetWidgetsGeometry(){
    WidgetWindow *ww = Graphics_GetBaseWidgetWindow();
    ww->ResetGeometry();
}

void DbgSupportStateChange(DbgState state, void *){
    WidgetWindow *ww = Graphics_GetBaseWidgetWindow();
    DbgWidgetButtons *bt = Graphics_GetDbgWidget();
    DbgWidgetExpressionViewer *vw = Graphics_GetDbgExpressionViewer();
    if(state == DbgState::Ready){
        // the debgger just initialized
        if(!ww->Contains(bt)){
            InitializeDbgButtons(ww, bt);
        }

        if(!ww->Contains(vw)){
            InitializeDbgExpressionViewer(ww, vw);
        }
    }else if(state == DbgState::Exited){
        ww->Erase(bt);
        ww->Erase(vw);
        DbgSupport_ResetBreakpointMap();
    }
}

void DbgSupport_ForEachBkpt(std::string path, std::function<void(DbgBkpt *)> callback){
    for(DbgBkpt &bkpt : appContext.dbgBreaks){
        if(path == bkpt.file){
            callback(&bkpt);
        }
    }
}

void DbgSupportBreakpointFeedback(BreakpointFeedback feedback, void *){
    if(feedback.rv){
        DbgSupport_ResetBreakpointMap();
        Dbg_GetBreakpointList(appContext.dbgBreaks);
        printf("Feedback at %s : %d ( %d ) => total : %d\n", feedback.bkpt.file.c_str(),
                feedback.bkpt.line, (int)feedback.bkpt.enabled,
                (int)appContext.dbgBreaks.size());
    }else{
        printf("Could not set breakpoint\n");
    }
}

void AppRemoveMouseHook(){
    appContext.mouseHook = AppEmptyMouseHook;
}

void AppSetMouseHook(std::function<void(void)> hook){
    appContext.mouseHook = hook;
}

void AppInitialize(){
    KeyboardInitMappings();
    AppInitializeFreeTypingBindings();
    AppInitializeQueryBarBindings();
    AppInitializeViewerBindings();

    KeyboardSetActiveMapping(appContext.freeTypeMapping);
    appContext.dbgHandle =
          DbgApp_RegisterStateChangeCallback(DbgSupportStateChange, nullptr);
    appContext.dbgBkptHandle =
          DbgApp_RegisterBreakpointFeedbackCallback(DbgSupportBreakpointFeedback, nullptr);

    AppRemoveMouseHook();
    //DualModeEnter();
}

void AppInitializeViewerBindings(){
    appContext.viewerMapping = InitializeViewerBindings();
}

void AppInitializeQueryBarBindings(){
    BindingMap *mapping = nullptr;
    mapping = KeyboardCreateMapping();
    RegisterKeyboardDefaultEntry(mapping, AppDefaultEntry, nullptr);
    RegisterRepeatableEvent(mapping, AppDefaultReturn, Key_Escape);
    RegisterRepeatableEvent(mapping, AppDefaultRemoveOne, Key_Backspace);

    RegisterRepeatableEvent(mapping, AppCommandQueryBarNext, Key_Down);
    RegisterRepeatableEvent(mapping, AppCommandQueryBarPrevious, Key_Up);

    RegisterRepeatableEvent(mapping, AppCommandQueryBarRightArrow, Key_Right);
    RegisterRepeatableEvent(mapping, AppCommandQueryBarLeftArrow, Key_Left);
    RegisterRepeatableEvent(mapping, AppCommandQueryBarJumpNext, Key_LeftControl, Key_Right);
    RegisterRepeatableEvent(mapping, AppCommandQueryBarJumpPrevious, Key_LeftControl, Key_Left);

    RegisterRepeatableEvent(mapping, AppCommandSwapView, Key_LeftAlt, Key_W);
    RegisterRepeatableEvent(mapping, AppCommandSwapView, Key_RightAlt, Key_W);
    RegisterRepeatableEvent(mapping, AppCommandQueryBarCommit, Key_Enter);
    RegisterRepeatableEvent(mapping, AppCommandPaste, Key_LeftControl, Key_V);

    appContext.queryBarMapping = mapping;
}

void AppMemoryDebugFreeze(){
#if defined(MEMORY_DEBUG)
    __memory_freeze();
    printf(" === === === FREEZED STATE\n");
#endif
}

void AppMemoryDebugCmp(){
#if defined(MEMORY_DEBUG)
    __memory_compare_state();
    getchar();
#endif
}

// TODO: Look, this thing is working nice however it might be interesting
//       to support dual mode for typing like vim or emacs. While I don't really
//       feel like this is something that is actually needed because the integration
//       is looking smooth enough to not have this be required, it might simplify
//       implementation of multi-line editing as we can pretty much disable all inputs
//       and not have to care about the states of each view. So ... maybe implement
//       a dual mode for more robust editing like multi-line tabs and erases?
//       It is also pretty easy to implement it since keyboard is handled by individual
//       mapping, so we can pretty much just create a 'dual.cpp' and implement custom
//       commands with a new different keyboard binding there and disable the default
//       entry and return routines, just like the small tmux we already have.
// TODO: Multi-line operations: edit/erase/tab/move
void AppInitializeFreeTypingBindings(){
    BindingMap *mapping = nullptr;
    mapping = KeyboardCreateMapping();

    RegisterKeyboardDefaultEntry(mapping, AppDefaultEntry, nullptr);
    RegisterRepeatableEvent(mapping, AppDefaultReturn, Key_Escape);
    RegisterRepeatableEvent(mapping, AppDefaultRemoveOne, Key_Backspace);
    RegisterRepeatableEvent(mapping, AppTerminate, Key_LeftAlt, Key_F4);

    //DEBUG KEYS
    RegisterRepeatableEvent(mapping, AppMemoryDebugFreeze, Key_LeftControl, Key_1);
    RegisterRepeatableEvent(mapping, AppMemoryDebugCmp, Key_LeftControl, Key_2);

    //FILE MANAGEMENT KEYS
    RegisterOnDragAndDropCallback(Graphics_GetGlobalWindow(), AppDragAndDrop, nullptr);
    RegisterRepeatableEvent(mapping, AppCommandOpenFile, Key_LeftAlt, Key_F);
    RegisterRepeatableEvent(mapping, AppCommandKillView, Key_LeftControl,
                            Key_LeftAlt, Key_K);
    RegisterRepeatableEvent(mapping, AppCommandKillBuffer, Key_LeftControl, Key_K);
    RegisterRepeatableEvent(mapping, AppCommandSaveBufferView, Key_LeftAlt, Key_S);
    RegisterRepeatableEvent(mapping, AppCommandStoreFileCommand, Key_LeftControl, Key_E);

    RegisterRepeatableEvent(mapping, AppCommandLeftArrow, Key_Left);
    RegisterRepeatableEvent(mapping, AppCommandRightArrow, Key_Right);
    RegisterRepeatableEvent(mapping, AppCommandUpArrow, Key_Up);
    RegisterRepeatableEvent(mapping, AppCommandDownArrow, Key_Down);

    RegisterRepeatableEvent(mapping, AppCommandSwitchTheme, Key_LeftControl, Key_T);

    RegisterRepeatableEvent(mapping, AppCommandSwitchFont, Key_LeftControl, Key_F);
    RegisterRepeatableEvent(mapping, AppCommandListFunctions, Key_RightControl, Key_F);
    RegisterRepeatableEvent(mapping, AppCommandJumpLeftArrow, Key_Left, Key_LeftControl);
    RegisterRepeatableEvent(mapping, AppCommandJumpRightArrow, Key_Right, Key_LeftControl);
    RegisterRepeatableEvent(mapping, AppCommandJumpUpArrow, Key_Up, Key_LeftControl);
    RegisterRepeatableEvent(mapping, AppCommandJumpDownArrow, Key_Down, Key_LeftControl);

    RegisterRepeatableEvent(mapping, AppCommandJumpLeftArrow, Key_Left, Key_LeftAlt);
    RegisterRepeatableEvent(mapping, AppCommandJumpRightArrow, Key_Right, Key_LeftAlt);
    RegisterRepeatableEvent(mapping, AppCommandJumpUpArrow, Key_Up, Key_LeftAlt);
    RegisterRepeatableEvent(mapping, AppCommandJumpDownArrow, Key_Down, Key_LeftAlt);

    RegisterRepeatableEvent(mapping, AppCommandRemovePreviousToken,
                            Key_Backspace, Key_LeftControl);
    RegisterRepeatableEvent(mapping, AppCommandRemovePreviousToken,
                            Key_Backspace, Key_LeftAlt);

    RegisterRepeatableEvent(mapping, AppCommandEnterKey, Key_Enter);

    RegisterRepeatableEvent(mapping, AppCommandHomeLine, Key_Home);
    RegisterRepeatableEvent(mapping, AppCommandEndLine, Key_End);
    RegisterRepeatableEvent(mapping, AppCommandSwapView, Key_LeftAlt, Key_W);
    RegisterRepeatableEvent(mapping, AppCommandSwapView, Key_RightAlt, Key_W);

    RegisterRepeatableEvent(mapping, AppCommandSetGhostCursor, Key_Space,
                            Key_LeftControl);
    RegisterRepeatableEvent(mapping, AppCommandSetGhostCursor, Key_Space,
                            Key_RightControl);

    RegisterRepeatableEvent(mapping, AppCommandAutoComplete, Key_LeftControl, Key_N);
    RegisterRepeatableEvent(mapping, AppCommandSwapLineNbs, Key_LeftAlt, Key_N);
    RegisterRepeatableEvent(mapping, AppCommandKillEndOfLine, Key_LeftAlt, Key_K);

    RegisterRepeatableEvent(mapping, AppCommandQueryBarSearch, Key_LeftControl, Key_S);
    RegisterRepeatableEvent(mapping, AppCommandQueryBarSearchAndReplace, Key_LeftControl, Key_O);
    //RegisterRepeatableEvent(mapping, AppCommandGitDiffCurrent, Key_LeftAlt, Key_O);
    RegisterRepeatableEvent(mapping, AppCommandQueryBarGotoLine, Key_LeftControl, Key_G);
    RegisterRepeatableEvent(mapping, AppCommandQueryBarGotoLine, Key_LeftAlt, Key_G);
    RegisterRepeatableEvent(mapping, AppCommandQueryBarRevSearch, Key_LeftControl, Key_R);

    RegisterRepeatableEvent(mapping, AppCommandSwitchBuffer, Key_LeftAlt, Key_B);
    RegisterRepeatableEvent(mapping, AppCommandInsertTab, Key_Tab);

    RegisterRepeatableEvent(mapping, AppCommandSplitHorizontal, Key_LeftAlt, Key_LeftControl, Key_H);
    RegisterRepeatableEvent(mapping, AppCommandSplitVertical, Key_LeftAlt, Key_LeftControl, Key_V);
    RegisterRepeatableEvent(mapping, AppCommandPaste, Key_LeftControl, Key_V);
    RegisterRepeatableEvent(mapping, AppCommandCopy, Key_LeftControl, Key_C);
    RegisterRepeatableEvent(mapping, AppCommandCopy, Key_LeftControl, Key_Q);
    RegisterRepeatableEvent(mapping, AppCommandSwapToBuildBuffer, Key_LeftControl, Key_M);

    RegisterRepeatableEvent(mapping, AppCommandUndo, Key_LeftControl, Key_Z);

    //TODO: re-do (AppCommandRedo)
    RegisterRepeatableEvent(mapping, AppCommandQueryBarInteractiveCommand,
                            Key_LeftControl, Key_Semicolon);

    RegisterRepeatableEvent(mapping, AppCommandJumpNesting, Key_RightControl, Key_J);
    RegisterRepeatableEvent(mapping, AppJumpToNextError, Key_LeftControl, Key_J);
    RegisterRepeatableEvent(mapping, AppCommandIndent, Key_LeftControl, Key_Tab);
    RegisterRepeatableEvent(mapping, AppCommandCut, Key_LeftControl, Key_W);
    //RegisterRepeatableEvent(mapping, AppCommandCut, Key_LeftControl, Key_X);

    // Let the control commands bind its things
    ControlCommands_Initialize();
    ControlCommands_BindTrigger(mapping);

    // Let dual controller do its thing as well
    DualModeInit();
    DualMode_BindTrigger(mapping);

    DbgApp_Initialize();

    appContext.freeTypeMapping = mapping;
}


void AppCommandJumpLeftArrow(){ AppCommandFreeTypingJumpToDirection(DIRECTION_LEFT); }
void AppCommandJumpRightArrow(){ AppCommandFreeTypingJumpToDirection(DIRECTION_RIGHT); }
void AppCommandJumpUpArrow(){ AppCommandFreeTypingJumpToDirection(DIRECTION_UP); }
void AppCommandJumpDownArrow(){ AppCommandFreeTypingJumpToDirection(DIRECTION_DOWN); }
void AppCommandLeftArrow(){ AppCommandFreeTypingArrows(DIRECTION_LEFT); }
void AppCommandRightArrow(){ AppCommandFreeTypingArrows(DIRECTION_RIGHT); }
void AppCommandUpArrow(){ AppCommandFreeTypingArrows(DIRECTION_UP); }
void AppCommandDownArrow(){ AppCommandFreeTypingArrows(DIRECTION_DOWN); }
