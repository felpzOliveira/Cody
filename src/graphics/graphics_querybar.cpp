#include <graphics.h>
#include <glad/glad.h>
#include <string.h>
#include <query_bar.h>
#include <view.h>

static void _Graphics_RenderTextAt(OpenGLState *state, Float &x, Float &y, 
                                   char *string, uint len, vec4i color,
                                   int *pGlyph)
{
    OpenGLFont *font = &state->font;
    x = fonsStashMultiTextColor(font->fsContext, x, y, color.ToUnsigned(),
                                string, string+len, pGlyph);
}

static void _Graphics_RenderText(OpenGLState *state, View *view, Theme *theme){
    Float x = 10;
    Float y = 0;
    int pGlyph = -1;
    QueryBar *bar = View_GetQueryBar(view);
    OpenGLFont *font = &state->font;
    char *header = nullptr;
    uint headerLen = 0;
    fonsClearState(font->fsContext);
    fonsSetSize(font->fsContext, font->fontMath.fontSizeAtRenderCall);
    fonsSetAlign(font->fsContext, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    
    glUseProgram(font->shader.id);
    Shader_UniformMatrix4(font->shader, "projection", &state->projection.m);
    Shader_UniformMatrix4(font->shader, "modelView", &state->scale.m);
    
    QueryBar_GetTitle(bar, &header, &headerLen);
    if(header){
        vec4i col = GetColor(theme, TOKEN_ID_FUNCTION);
        _Graphics_RenderTextAt(state, x, y, header, headerLen, col, &pGlyph);
    }
    
    QueryBar_GetWrittenContent(bar, &header, &headerLen);
    if(headerLen > 0 && header){
        vec4i col = GetColor(theme, TOKEN_ID_NONE);
        _Graphics_RenderTextAt(state, x, y, header, headerLen, col, &pGlyph);
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
}

void Graphics_RenderQueryBarCursor(OpenGLState *state, QueryBar *bar, vec4f color){
    Float x0 = 0, x1 = 0, y0 = 0, y1 = 0;
    uint rawp = 0;
    int pGlyph = -1;
    int utf8Len = 1;
    int pUtf8Len = 1;
    OpenGLFont *font = &state->font;
    Buffer *buffer = &bar->buffer;
    char *ptr = buffer->data;
    uint pos = bar->cursor.textPosition.y;
    
    rawp = Buffer_Utf8PositionToRawPosition(buffer, pos-1, &utf8Len);
    x0 = fonsComputeStringAdvance(state->font.fsContext, ptr, rawp+utf8Len, &pGlyph);
    x0 += 10;
    
    rawp = Buffer_Utf8PositionToRawPosition(buffer, pos, &pUtf8Len);
    x1 = x0 + fonsComputeStringAdvance(state->font.fsContext, &ptr[rawp],
                                       pUtf8Len, &pGlyph);
    
    y1 = y0 + state->font.fontMath.fontSizeAtRenderCall;
    
    glUseProgram(font->cursorShader.id);
    
    Shader_UniformMatrix4(font->cursorShader, "projection", &state->projection.m);
    Shader_UniformMatrix4(font->cursorShader, "modelView", &state->scale.m);
    
    Graphics_QuadPush(state, vec2ui(x0, y0), vec2ui(x1, y1), color);
}

void Graphics_RenderQueryBarSelection(View *view, OpenGLState *state, Theme *theme){
    ViewState vstate = View_GetState(view);
    QueryBar *bar = View_GetQueryBar(view);
    if(vstate != View_QueryBar) return;
    OpenGLFont *font = &state->font;
    ActivateViewportAndProjection(state, view, ViewportText);
    QueryBarCommand cmd = QueryBar_GetActiveCommand(bar);
    if(cmd == QUERY_BAR_CMD_SEARCH || cmd == QUERY_BAR_CMD_REVERSE_SEARCH){
        QueryBarCmdSearch *result = nullptr;
        QueryBar_GetSearchResult(bar, &result);
        
        if(result->valid == 0) return;
        
        char *searched = nullptr;
        uint slen = 0;
        QueryBar_GetWrittenContent(bar, &searched, &slen);
        
        if(slen == 0) return;
        
        // 1- Grab the buffer and position inside the buffer
        BufferView *bView = View_GetBufferView(view);
        vec2ui lines = BufferView_GetViewRange(bView);
        Buffer *buffer = BufferView_GetBufferAt(bView, result->lineNo);
        uint p8 = Buffer_Utf8RawPositionToPosition(buffer, result->position);
        uint e8 = Buffer_Utf8RawPositionToPosition(buffer, result->position + 
                                                   result->length);
        
        AssertA(e8 > p8, "Zero QueryBar result");
        
        // 2- I think this model is correct because cursor will jump
        //    to the line when the QueryBar triggers a search. This means
        //    rendering after the BufferView will have already computed
        //    cursor projection and view->horizontal will be ok.
        Scroll *scroll = &bView->scroll;
        Transform model = state->scale * scroll->horizontal;
        
        // 3- Compute coordinates
        Float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        uint rawp = 0, orawp = 0;
        int len8 = 1, plen8 = 1;
        int pGlyph = -1;
        int oGlyph = -1;
        if(p8 > 0){
            orawp = Buffer_Utf8PositionToRawPosition(buffer, p8-1, &len8);
            x0 = fonsComputeStringAdvance(state->font.fsContext, buffer->data,
                                          orawp+len8, &pGlyph);
            oGlyph = pGlyph;
        }
        
        rawp = Buffer_Utf8PositionToRawPosition(buffer, e8-1, &plen8);
        x1 = x0 + fonsComputeStringAdvance(state->font.fsContext, &buffer->data[rawp],
                                           result->length, &pGlyph);
        
        y0 = (result->lineNo - lines.x) * state->font.fontMath.fontSizeAtRenderCall;
        if(view->descLocation == DescriptionTop){
            y0 += state->font.fontMath.fontSizeAtRenderCall;
        }
        
        y1 = y0 + state->font.fontMath.fontSizeAtRenderCall;
        
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
        //printf("%s at (%u %u) ( %u )\n", searched, result->lineNo, p8, slen);
        
        Graphics_PrepareTextRendering(state, &state->projection, &model);
        Graphics_PushText(state, x0, y0, searched, slen, wcolor, &oGlyph);
        Graphics_FlushText(state);
    }
}

int Graphics_RenderQueryBar(View *view, OpenGLState *state,
                            Theme *theme, Float dt)
{
    QueryBar *bar = View_GetQueryBar(view);
    OpenGLFont *font = &state->font;
    Geometry *geometry = &bar->geometry;
    vec4f cursorColor = GetUIColorf(theme, UIQueryBarCursor);
    
    Float lineSize = ((geometry->upper.x - geometry->lower.x) * 
                      font->fontMath.invReduceScale);
    
    vec4f backgroundColor = GetUIColorf(theme, UISelectorBackground);
    
    Float fcol[] = { backgroundColor.x, backgroundColor.y, backgroundColor.z,
        backgroundColor.w };
    
    ActivateViewportAndProjection(state, geometry);
    
    glClearBufferfv(GL_COLOR, 0, fcol);
    glClearBufferfv(GL_DEPTH, 0, kOnes);
    
    // Check if we need to render something for the list
    if(View_GetState(view) == View_SelectableList){
        int active = View_SelectableListGetActiveIndex(view);
        if(active < 0){
            vec4f cc = cursorColor * 0.2;
            fcol[0] = cc[0];
            fcol[1] = cc[1];
            fcol[2] = cc[2];
            fcol[3] = cc[3];
            glClearBufferfv(GL_COLOR, 0, fcol);
        }
    }
    
    Graphics_RenderQueryBarCursor(state, bar, cursorColor);
    Graphics_QuadFlush(state);
    
    _Graphics_RenderText(state, view, theme);
    
    Graphics_RenderQueryBarSelection(view, state, theme);
    
    glDisable(GL_SCISSOR_TEST);
    return 0;
}