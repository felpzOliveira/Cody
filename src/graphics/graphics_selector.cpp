#include <graphics.h>
#include <glad/glad.h>
#include <string.h>
#include <view.h>
#include <autocomplete.h>
#include <file_provider.h>

typedef struct FrameStyle{
    int with_border;
    int with_line_border;
    int with_load;
    int with_line_highlight;
    vec4f item_background_color;
    vec4i item_foreground_color;
    vec4i item_load_color;
    vec4f item_active_border_color;
    vec4i item_active_foreground_color;
    vec4f item_active_background_color;
    Float yScaling;
}FrameStyle;

///////////////////////////////////////////////////////////////
//TODO > missing line during buffer selection on vertical split
//TODO > pratical way to handle many icons (load all of them or go caching?)
///////////////////////////////////////////////////////////////

void RenderSelectableListItensBackground(OpenGLState *state, SelectableList *list,
                                         Theme *theme, Float lWidth, FrameStyle *style)
{
    vec2ui range = SelectableList_GetViewRange(list);
    Float y0 = 0;
    for(uint i = range.x; i < range.y; i++){
        Float y1 = y0 + style->yScaling * state->font.fontMath.fontSizeAtRenderCall;

        Graphics_QuadPush(state, vec2ui(0, y0), vec2ui(lWidth, y1),
                          style->item_background_color);

        if((int)i == list->active && style->with_line_border){
            int w = GetSelectorBorderWidth(theme) - (1-useDriverDownscale) * 2;
            Graphics_QuadPushBorder(state, 0, y0, lWidth, y1, w,
                                    style->item_active_border_color);
        }

        if((int)i == list->active && style->with_line_highlight){
            int w = 2; // TODO: should this be in theme?
            Graphics_QuadPush(state, vec2ui(w, y0+w), vec2ui(lWidth-w, y1-w),
                              style->item_active_background_color);
        }

        y0 += (1.0 + kViewSelectableListOffset) * style->yScaling *
            state->font.fontMath.fontSizeAtRenderCall;
    }
}

void RenderSelectableListItens(View *view, OpenGLState *state,
                               SelectableList *list,
                               Theme *theme, Float lWidth, FrameStyle *style,
                               FileOpener *opener = nullptr)
{
    vec2ui range = SelectableList_GetViewRange(list);
    Float y0 = 0;
    Float xOffset = useDriverDownscale ? 16 : 4;

    EncoderDecoder *encoder = UTF8Encoder();
    Graphics_PrepareTextRendering(state, &state->projection, &state->scale);
    uint maxn = 0;
    char textBuffer[64];
    uint textBufferSize = 64;
    for(uint i = range.x; i < range.y; i++){
        Buffer *buffer = nullptr;
        SelectableList_GetItem(list, i, &buffer);
        AssertA(buffer != nullptr, "Failed to get buffer from selectable list");
        maxn = Max(buffer->taken, maxn);
    }

    for(uint i = range.x; i < range.y; i++){
        Buffer *buffer = nullptr;

        int pGlyph = -1;
        Float x = xOffset;
        Float y1 = y0 + style->yScaling * state->font.fontMath.fontSizeAtRenderCall;
        Float ym = (y1 + y0) * 0.5 - 0.4 * state->font.fontMath.fontSizeAtRenderCall;

        uint rindex = SelectableList_GetRealIndex(list, i);

        SelectableList_GetItem(list, i, &buffer);
        AssertA(buffer != nullptr, "Invalid buffer returned by View_SelectableListGetItem");

        uint ni = 0;
        if(view->allowPathCompression){
            ni = GetSimplifiedPathName(buffer->data, buffer->taken);
        }
        char *dataPtr = &buffer->data[ni];
        uint len = buffer->taken - ni;
        vec4i s_col = (int)i == list->active ? style->item_active_foreground_color :
                        style->item_foreground_color;

        // Fetch the extension name
        if(opener && style->with_load && opener->entries){
            FileEntry *e = &opener->entries[rindex];
            if(useIcons == 0){
                vec4i e_col;
                uint extLen =
                    Graphics_FetchExtensionRenderProps(textBuffer,
                                                       textBufferSize,
                                                       e_col, e);
                Graphics_PushText(state, x, ym, textBuffer, extLen,
                                  e_col, &pGlyph, encoder);
            }else{
                uint mid = 0;
                int off = 0;
                mid = Graphics_FetchTextureFor(state, e, &off);

                Float size = 0.8f * (y1-y0);
                vec2ui p(x, ym - 0.15f * (y1-y0));

                int needs_render = Graphics_ImagePush(state, p, p+size, mid);
                if(needs_render){
                    // before flushing the images we need to flush the quad
                    // otherwise later flushes will not display images
                    glUseProgram(state->font.cursorShader.id);
                    Graphics_QuadFlush(state);

                    // now can flush the image
                    Graphics_ImageFlush(state);

                    // push the new image
                    Graphics_ImagePush(state, p, p+size, mid);
                }

                x += 1.5f * size;
            }
        }

        Graphics_PushText(state, x, ym, dataPtr, len, s_col, &pGlyph, encoder);

        if(view->bufferFlags.size() > 0 && rindex < view->bufferFlags.size()){
            if(view->bufferFlags[rindex] > 0){
                s_col.w *= 0.5;
                std::string value = "  (" + std::string(buffer->data, ni-1) + ")";
                Graphics_PushText(state, x, ym, (char *)value.c_str(),
                                  value.size(), s_col, &pGlyph, encoder);
            }
        }

        if(opener && style->with_load && opener->entries){
            FileEntry *e = &opener->entries[rindex];
            if(e->isLoaded){
                const char *ld = " LOADED *";
                uint llen = 9;
                if(!FileProvider_IsLineBufferDirty(dataPtr, len)){
                    llen = 7;
                }

                Graphics_PushText(state, x, ym, (char *)ld, llen,
                                style->item_load_color, &pGlyph, encoder);
            }
        }

        y0 += (1.0 + kViewSelectableListOffset) * style->yScaling *
            state->font.fontMath.fontSizeAtRenderCall;
    }
}

int Graphics_RenderHoverableList(View *view, OpenGLState *state, Theme *theme,
                                 Geometry *geometry, SelectableList *list)
{
    OpenGLFont *font = &state->font;
    vec4f backColor = GetUIColorf(theme, UIBackground);
    ActivateViewportAndProjection(state, geometry);
    Float lWidth = (geometry->upper.x - geometry->lower.x) * font->fontMath.invReduceScale;
    Float lHeight = (geometry->upper.y - geometry->lower.y) * font->fontMath.invReduceScale;

    Float fcol[] = { backColor.x, backColor.y, backColor.z, backColor.w };
    glClearBufferfv(GL_COLOR, 0, fcol);
    glClearBufferfv(GL_DEPTH, 0, kOnes);

    vec4i tcol = GetColor(theme, TOKEN_ID_NONE);
    vec4i scol = GetUIColor(theme, UIHoverableListItem);
    vec4f sscol = GetUIColorf(theme, UIHoverableListItemBackground);

    FrameStyle style = {
        .with_line_border = 0,
        .with_load = 0,
        .with_line_highlight = 1,
        .item_background_color = backColor,
        .item_foreground_color = tcol,
        .item_load_color = vec4i(0),
        .item_active_border_color = vec4f(0),
        .item_active_foreground_color = scol,
        .item_active_background_color = sscol,
        .yScaling = kAutoCompleteListScaling,
    };

    RenderSelectableListItensBackground(state, list, theme, lWidth, &style);

    Graphics_QuadPushBorder(state, 0, 0, lWidth, lHeight, 2,
                            ColorFromHexf(0x448589f6)); // theme?

    glUseProgram(font->cursorShader.id);

    Shader_UniformMatrix4(font->cursorShader, "projection", &state->projection.m);
    Shader_UniformMatrix4(font->cursorShader, "modelView", &state->scale.m);

    Graphics_QuadFlush(state);

    RenderSelectableListItens(view, state, list, theme, lWidth, &style);

    Graphics_PrepareTextRendering(state, &state->projection, &state->scale);
    Graphics_FlushText(state);

    glDisable(GL_SCISSOR_TEST);
    return 0;
}

int Graphics_RenderAutoComplete(View *view, OpenGLState *state, Theme *theme, Float dt){
    Geometry geo;
    Geometry geometry = view->geometry;
    OpenGLFont *font = &state->font;
    SelectableList *list = view->autoCompleteList;
    Buffer *largeBuffer = nullptr;
    uint maxn = 0;
    int glyph = -1;

    Float w = geometry.upper.x - geometry.lower.x;
    Float h = geometry.upper.y - geometry.lower.y;
    Float yoff = 0;

    BufferView *bView = View_GetBufferView(view);
    vec2ui range = BufferView_GetViewRange(bView);
    LineBuffer *lineBuffer = SelectableList_GetLineBuffer(list);
    vec2ui cursor = BufferView_GetCursorPosition(bView);

    yoff = font->fontMath.fontSizeAtDisplay * (lineBuffer->lineCount + 0.5) * kAutoCompleteListScaling;
    for(uint i = 0; i < lineBuffer->lineCount; i++){
        Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, i);
        if(maxn < buffer->taken){
            largeBuffer = buffer;
            maxn = buffer->taken;
        }
    }

    yoff = Min(yoff, kAutoCompleteMaxHeightFraction * h);

    Float fw = 0;
    if(largeBuffer){
        char *ptr = largeBuffer->data;
        int end = largeBuffer->taken;
        fw = fonsComputeStringAdvance(font->fsContext, ptr, end, &glyph,
                                      LineBuffer_GetEncoderDecoder(lineBuffer));
    }

    Float gw = kAutoCompleteListScaling * fw * font->fontMath.reduceScale;
    AssertA(w > gw, "View is too small!");

    OpenGLCursor *glCursor = &state->glCursor;
    Float x = glCursor->pMin.x * font->fontMath.reduceScale;
    if(x + gw > w){
        x = w - gw;
    }

    Float inc = view->descLocation == DescriptionTop ? 1.0 : 0.0;

    Float popHeight = h - (cursor.x - range.x + inc + 1.0) * font->fontMath.fontSizeAtDisplay - yoff;
    if(popHeight < 0){
        popHeight = h - (cursor.x - range.x + inc) * font->fontMath.fontSizeAtDisplay;
        AssertA(popHeight > 0, "Failed to compute popup y position");
    }

    geo.lower.x = geometry.lower.x + x;
    geo.lower.y = geometry.lower.y + popHeight;
    geo.upper.x = geo.lower.x + gw;
    geo.upper.y = geo.lower.y + yoff;

    return Graphics_RenderHoverableList(view, state, theme, &geo, list);
}

vec2i Graphics_ComputeSelectableListItem(OpenGLState *state, uint y, View *view){
    Geometry geometry;
    OpenGLFont *font = &state->font;
    if(!view) return vec2f(0);

    BufferView *bView = View_GetBufferView(view);
    if(!bView) return vec2f(0);

    SelectableList *list = &view->selectableList;

    BufferView_GetGeometry(bView, &geometry);
    vec2ui range = SelectableList_GetViewRange(list);
    Float y0 = 0;
    int it = 0;
    Float yScaling = kViewSelectableListScaling;
    vec2i res(-1);
    for(uint i = range.x; i < range.y; i++){
        Float y1 = y0 + yScaling * font->fontMath.fontSizeAtRenderCall;
        if(y >= y0 && y <= y1){
            res = vec2i(it, i);
            break;
        }

        y0 += (1.0 + kViewSelectableListOffset) * yScaling *
            font->fontMath.fontSizeAtRenderCall;
        it += 1;
    }

    return res;
}

int Graphics_RenderListSelector(View *view, OpenGLState *state, Theme *theme, Float dt){
    OpenGLFont *font = &state->font;
    FileOpener *opener = View_GetFileOpener(view);

    Geometry geometry = view->geometry;
    SelectableList *list = &view->selectableList;

    vec4f backgroundColor = GetUIColorf(theme, UIBackground);
    Float fcol[] = { backgroundColor.x, backgroundColor.y,
                     backgroundColor.z, backgroundColor.w };

    Float lWidth = (geometry.upper.x - geometry.lower.x) *
                        font->fontMath.invReduceScale;

    ActivateViewportAndProjection(state, view, ViewportFrame);
    glClearBufferfv(GL_COLOR, 0, fcol);
    glClearBufferfv(GL_DEPTH, 0, kOnes);

    vec4f col = GetUIColorf(theme, UISelectableListBackground);
    vec4i tcol = GetColor(theme, TOKEN_ID_NONE);
    vec4i tcolSel = GetUIColor(theme, UISelectorLoadedColor);
    vec4f scol = GetUIColorf(theme, UISelectorLoadedColor);

    FrameStyle style = {
        .with_line_border = 1,
        .with_load = 1,
        .with_line_highlight = 0,
        .item_background_color = col,
        .item_foreground_color = tcol,
        .item_load_color = tcolSel,
        .item_active_border_color = scol,
        .item_active_foreground_color = tcol,
        .yScaling = kViewSelectableListScaling,
    };

    uint currFontSize = state->font.fontMath.fontSizeAtDisplay;


    //printf("Range: %u - %u\n", range.x, range.y);

    //Graphics_SetFontSize(state, currFontSize + 2);

    glUseProgram(font->cursorShader.id);
    Shader_UniformMatrix4(font->cursorShader, "projection", &state->projection.m);
    Shader_UniformMatrix4(font->cursorShader, "modelView", &state->scale.m);

    glUseProgram(state->imageShader.id);
    Shader_UniformMatrix4(state->imageShader, "projection", &state->projection.m);
    Shader_UniformMatrix4(state->imageShader, "modelView", &state->scale.m);

    RenderSelectableListItensBackground(state, list, theme, lWidth, &style);

    glUseProgram(font->cursorShader.id);
    Graphics_QuadFlush(state);

    RenderSelectableListItens(view, state, list, theme, lWidth, &style, opener);

    Graphics_PrepareTextRendering(state, &state->projection, &state->scale);
    Graphics_FlushText(state);

    Graphics_ImageFlush(state);

    Graphics_SetFontSize(state, currFontSize);
    glDisable(GL_SCISSOR_TEST);
    return 0;
}
