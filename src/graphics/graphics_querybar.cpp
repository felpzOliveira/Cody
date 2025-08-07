#include <graphics.h>
#include <glad/glad.h>
#include <string.h>
#include <query_bar.h>
#include <view.h>
#include <app.h>

static void _Graphics_RenderTextAt(OpenGLState *state, Float &x, Float &y,
                                   char *string, uint len, vec4i color,
                                   int *pGlyph, EncoderDecoder *encoder)
{
    OpenGLFont *font = &state->font;
    x = fonsStashMultiTextColor(font->fsContext, x, y, color.ToUnsigned(),
                                string, string+len, pGlyph, encoder);
}

static void _Graphics_RenderText(OpenGLState *state, View *view, Theme *theme, vec4f data){
    Float x = 10;
    Float y = 0;
    int pGlyph = -1;
    QueryBar *bar = View_GetQueryBar(view);
    Buffer *buffer = &bar->buffer;
    EncoderDecoder *encoder = &bar->encoder;
    OpenGLFont *font = &state->font;
    char *header = nullptr;
    uint headerLen = 0;
    std::string headerstr;

    fonsClearState(font->fsContext);
    fonsSetSize(font->fsContext, font->fontMath.fontSizeAtRenderCall);
    fonsSetAlign(font->fsContext, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(font->shader.id);
    Shader_UniformMatrix4(font->shader, "projection", &state->projection.m);
    Shader_UniformMatrix4(font->shader, "modelView", &state->scale.m);

    QueryBar_GetTitle(bar, &header, &headerLen);
    //QueryBar_GetRenderTitle(bar, headerstr);

    if(header){
        vec4i col = GetColor(theme, TOKEN_ID_NONE);
        _Graphics_RenderTextAt(state, x, y, header, headerLen, col, &pGlyph, encoder);
    }

    //QueryBar_GetWrittenContent(bar, &header, &headerLen);
    (void) QueryBar_GetRenderContent(bar, headerstr);
    header = (char *)headerstr.c_str();
    headerLen = headerstr.size();

    if(headerLen > 0 && header){
        vec4i col = GetColor(theme, TOKEN_ID_NONE);
        _Graphics_RenderTextAt(state, x, y, header, headerLen, col, &pGlyph, encoder);
    }

#if 0
    //NOTE: I'm not sure we can actually do hinting as 
    //      our string search methods match anything and not
    //      the 'start' of a string, this leaves us with a corrupted displayable text.
    if(View_GetState(view) == View_SelectableList){
        Buffer *buffer = nullptr;
        Float x0 = x, y0 = y;
        uint active = View_SelectableListGetActiveIndex(view);
        uint rindex = View_SelectableListGetRealIndex(view, active);
        View_SelectableListGetItem(view, rindex, &buffer);

        // make sure we dont get any paths
        uint s = GetSimplifiedPathName(header, headerLen);
        uint at = GetSimplifiedPathName(buffer->data, buffer->taken);
        uint hintlen = buffer->taken - at;
        uint reallen = headerLen - s;
        if(hintlen > reallen){
            vec4i col = GetColor(theme, TOKEN_ID_NONE);
            col.w *= 0.5;

            //TODO: UTF-8
            _Graphics_RenderTextAt(state, x0, y0, &buffer->data[at+reallen],
                                   1, vec4i(0,0,0,255), &pGlyph);

            if(buffer->taken - reallen - at > 1){
                _Graphics_RenderTextAt(state, x0, y0, &buffer->data[at+1],
                                       buffer->taken - at - reallen - 1, col, &pGlyph);
            }
        }
    }
#endif

    fonsStashFlush(font->fsContext);
    glDisable(GL_BLEND);

    if(bar->cursor.textPosition.y < buffer->count && AppGetCursorStyle() == CURSOR_RECT){
        Float x0 = data.x;
        Float y0 = data.y;
        uint rawp = (uint)data.z;
        int baseGlyph = (int)data.w;
        Graphics_PrepareTextRendering(state, &state->projection, &state->model);
        vec4i cc = GetUIColor(defaultTheme, UICharOverCursor);
        char *chr = &buffer->data[rawp];
        fonsStashMultiTextColor(font->fsContext, x0, y0, cc.ToUnsigned(),
                                chr, chr+1, &baseGlyph, encoder);
        Graphics_FlushText(state);
    }
}

static vec4f Graphics_RenderQueryBarCursor(OpenGLState *state, QueryBar *bar, vec4f color){
    Float x0 = 0, x1 = 0, y0 = 0, y1 = 0;
    uint rawp = 0;
    int pGlyph = -1;
    int utf8Len = 1;
    int pUtf8Len = 1;
    std::string headerstr;
    OpenGLFont *font = &state->font;
    Buffer *buffer = &bar->buffer;
    char *ptr = buffer->data;
    uint pos = bar->cursor.textPosition.y;
    int baseGlyph = -1;
    EncoderDecoder *encoder = &bar->encoder;

    uint diff = QueryBar_GetRenderContent(bar, headerstr);

    rawp = Buffer_Utf8PositionToRawPosition(buffer, pos-1, &utf8Len, encoder);
    rawp -= diff;

    x0 = fonsComputeStringAdvance(state->font.fsContext, ptr, rawp+utf8Len, &pGlyph, encoder);
    x0 += 10;

    baseGlyph = pGlyph;

    rawp = Buffer_Utf8PositionToRawPosition(buffer, pos, &pUtf8Len, encoder);
    x1 = x0 + fonsComputeStringAdvance(state->font.fsContext, &ptr[rawp],
                                       pUtf8Len, &pGlyph, encoder);

    y1 = y0 + state->font.fontMath.fontSizeAtRenderCall;

    glUseProgram(font->cursorShader.id);

    Shader_UniformMatrix4(font->cursorShader, "projection", &state->projection.m);
    Shader_UniformMatrix4(font->cursorShader, "modelView", &state->scale.m);

    Graphics_RenderCursorAt(&state->glQuadBuffer, x0, y0, x1, y1, color, 3, 1);
    return vec4f(x0, y0, rawp, baseGlyph);
}

void Graphics_RenderQueryBarSelection(View *view, OpenGLState *state, Theme *theme){
    ViewState vstate = View_GetState(view);
    QueryBar *bar = View_GetQueryBar(view);
    if(vstate != View_QueryBar) return;
    OpenGLFont *font = &state->font;
    ActivateViewportAndProjection(state, view, ViewportText);
    QueryBarCommand cmd = QueryBar_GetActiveCommand(bar);
    if(cmd == QUERY_BAR_CMD_SEARCH || cmd == QUERY_BAR_CMD_REVERSE_SEARCH
       || cmd == QUERY_BAR_CMD_SEARCH_AND_REPLACE)
    {
        QueryBarCmdSearch *result = nullptr;
        QueryBarCmdSearchAndReplace *replace = &bar->replaceCmd;
        QueryBar_GetSearchResult(bar, &result);

        if(result->valid == 0) return;

        char *searched = nullptr;
        uint slen = 0;
        if(cmd == QUERY_BAR_CMD_SEARCH_AND_REPLACE){
            searched = replace->toLocate;
            slen = replace->toLocateLen;
        }else{
            QueryBar_GetWrittenContent(bar, &searched, &slen);
        }

        if(slen == 0) return;

        // 1- Grab the buffer and position inside the buffer
        BufferView *bView = View_GetBufferView(view);
        vec2ui lines = BufferView_GetViewRange(bView);
        Buffer *buffer = BufferView_GetBufferAt(bView, result->lineNo);
        EncoderDecoder *encoder = LineBuffer_GetEncoderDecoder(bView->lineBuffer);
        uint p8 = Buffer_Utf8RawPositionToPosition(buffer, result->position, encoder);

        // 2- I think this model is correct because cursor will jump
        //    to the line when the QueryBar triggers a search. This means
        //    rendering after the BufferView will have already computed
        //    cursor projection and view->horizontal will be ok.
        Scroll *scroll = &bView->scroll;
        Transform model = state->scale * scroll->horizontal;

        // 3- Compute coordinates
        Float padding = 0;
    #if defined(_WIN32)
        padding = 3;
    #endif
        Float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        uint orawp = 0;
        int len8 = 1;
        int pGlyph = -1;
        int oGlyph = -1;

        if(p8 > 0){
            orawp = Buffer_Utf8PositionToRawPosition(buffer, p8-1, &len8, encoder);
            x0 = fonsComputeStringAdvance(state->font.fsContext, buffer->data,
                                          orawp+len8, &pGlyph, encoder);
            oGlyph = pGlyph;
        }

        x0 += padding;

        // since there is a match we don't actually need to compute the raw position
        // for the searched string, we can simply copy the already filled string
        // from the search context which is better since it starts at 0 and is null
        // terminated
        x1 = x0 + fonsComputeStringAdvance(state->font.fsContext, &searched[0],
                                           result->length, &pGlyph, encoder);

        y0 = (result->lineNo - lines.x) * state->font.fontMath.fontSizeAtRenderCall;
        if(view->descLocation == DescriptionTop){
            y0 += state->font.fontMath.fontSizeAtRenderCall;
        }

        y1 = y0 + state->font.fontMath.fontSizeAtRenderCall;

        y0 += (1 - useDriverDownscale) * 2;
        y1 += (1 - useDriverDownscale) * 2;
        x0 += (1 - useDriverDownscale) * 2;
        x1 += (1 - useDriverDownscale) * 2;

        // 4- Render line highlight
        glUseProgram(font->cursorShader.id);
        Shader_UniformMatrix4(font->cursorShader, "projection", &state->projection.m);
        Shader_UniformMatrix4(font->cursorShader, "modelView", &model.m);

        vec4f color = GetUIColorf(theme, UISearchBackground);
        Graphics_QuadPush(state, vec2ui(x0, y0), vec2ui(x1, y1), color);
        Graphics_QuadFlush(state);

        // 5- Re-render the text
        vec4i wcolor = GetUIColor(theme, UISearchWord);

        slen = result->length;
        //printf("%s at (%u %u) ( %u - %u )\n", searched, result->lineNo,
                //p8, slen, (uint)strlen(searched));

        Graphics_PrepareTextRendering(state, &state->projection, &model);
        Graphics_PushText(state, x0, y0, searched, slen, wcolor, &oGlyph, encoder);
        Graphics_FlushText(state);
    }
}

int Graphics_RenderQueryBar(View *view, OpenGLState *state,
                            Theme *theme, Float dt)
{
    QueryBar *bar = View_GetQueryBar(view);
    Geometry *geometry = &bar->geometry;
    vec4f cursorColor = GetUIColorf(theme, UIQueryBarCursor);

    vec4f backgroundColor = GetUIColorf(theme, UISelectorBackground);

    Float fcol[] = { backgroundColor.x, backgroundColor.y, backgroundColor.z,
        backgroundColor.w };

    ActivateViewportAndProjection(state, geometry);

    glClearBufferfv(GL_COLOR, 0, fcol);
    glClearBufferfv(GL_DEPTH, 0, kOnes);

    // Check if we need to render something for the list
    if(View_GetState(view) == View_SelectableList){
        int active = View_SelectableListGetActiveIndex(view);
        if(active < 0){ // 51, 13, 13, 51
            vec4f cc = GetUIColorf(theme, UIQueryBarTypeColor);
            fcol[0] = cc[0];
            fcol[1] = cc[1];
            fcol[2] = cc[2];
            fcol[3] = cc[3];
            glClearBufferfv(GL_COLOR, 0, fcol);
        }
    }

    vec4f data = Graphics_RenderQueryBarCursor(state, bar, cursorColor);
    Graphics_QuadFlush(state);

    _Graphics_RenderText(state, view, theme, data);

    Graphics_RenderQueryBarSelection(view, state, theme);

    glDisable(GL_SCISSOR_TEST);
    return 0;
}
