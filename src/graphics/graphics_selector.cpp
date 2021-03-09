#include <graphics.h>
#include <glad/glad.h>
#include <string.h>
#include <view.h>

int Graphics_RenderHoverableList(View *view, OpenGLState *state, Theme *theme,
                                 Geometry *geometry, LineBuffer *lineBuffer)
{
    //OpenGLFont *font = &state->font;
    vec4f backColor = ColorFromHexf(0xff2f317c);
    ActivateViewportAndProjection(state, geometry);
    //Float lWidth = (geometry->upper.x - geometry->lower.x) * font->fontMath.invReduceScale;

    Float fcol[] = {backColor.x, backColor.y, backColor.z, backColor.w};
    glClearBufferfv(GL_COLOR, 0, fcol);
    glClearBufferfv(GL_DEPTH, 0, kOnes); //???


    glDisable(GL_SCISSOR_TEST);
    return 0;
}

int Graphics_RenderListSelector(View *view, OpenGLState *state,
                                Theme *theme, Float dt)
{
    Float scaling = kViewSelectableListScaling;
    OpenGLFont *font = &state->font;
    FileOpener *opener = View_GetFileOpener(view);
    
    Geometry geometry = view->geometry;
    SelectableList *list = &view->selectableList;
    
    Float lWidth = (geometry.upper.x - geometry.lower.x) * font->fontMath.invReduceScale;
    vec4f backgroundColor = GetUIColorf(theme, UIBackground);
    
    Float fcol[] = { backgroundColor.x, backgroundColor.y, backgroundColor.z, backgroundColor.w };
    
    ActivateViewportAndProjection(state, view, ViewportFrame);
    glClearBufferfv(GL_COLOR, 0, fcol);
    glClearBufferfv(GL_DEPTH, 0, kOnes);
    
    Float y0 = 0;
    //
    vec4f col = GetUIColorf(theme, UISelectableListBackground);
    vec4i tcol = GetColor(theme, TOKEN_ID_NONE);
    vec4i tcolSel = ColorFromHex(0xFFCB9401);
    vec4f scol = GetUIColorf(theme, UIQueryBarCursor);
    
    uint currFontSize = state->font.fontMath.fontSizeAtDisplay;
    vec2ui range = View_SelectableListGetViewRange(view);
    
    //printf("Range: %u - %u\n", range.x, range.y);
    
    //Graphics_SetFontSize(state, currFontSize + 2);
    Graphics_PrepareTextRendering(state, &state->projection, &state->scale);
    
    uint maxn = 0;
    for(uint i = range.x; i < range.y; i++){
        Buffer *buffer = nullptr;
        View_SelectableListGetItem(view, i, &buffer);
        AssertA(buffer != nullptr, "Failed to get buffer from selectable list");
        maxn = Max(buffer->taken, maxn);
    }
    
    for(uint i = range.x; i < range.y; i++){
        Buffer *buffer = nullptr;
        int pGlyph = -1;
        Float x = lWidth * 0.03;
        Float y1 = y0 + scaling * state->font.fontMath.fontSizeAtRenderCall;
        Float ym = (y1 + y0) * 0.5 - 0.5 * state->font.fontMath.fontSizeAtRenderCall;
        
        uint rindex = View_SelectableListGetRealIndex(view, i);
        
        View_SelectableListGetItem(view, i, &buffer);
        AssertA(buffer != nullptr, "Invalid buffer returned by View_SelectableListGetItem");
#if 0
        if(i != list->active){
            Graphics_QuadPush(state, vec2ui(0, y0), vec2ui(lWidth, y1), col);
            Graphics_PushText(state, x, ym, buffer->data, buffer->taken, tcol, &pGlyph);
        }else{
            Graphics_QuadPush(state, vec2ui(0, y0), vec2ui(lWidth, y1), scol);
            Graphics_PushText(state, x, ym, buffer->data, buffer->taken,
                              vec4i(255), &pGlyph);
        }
#else
        Graphics_QuadPush(state, vec2ui(0, y0), vec2ui(lWidth, y1), col);
        Graphics_PushText(state, x, ym, buffer->data, buffer->taken, tcol, &pGlyph);
        
        //TODO: This needs state and not do for any available list pointer
        if(opener->entries){
            FileEntry *e = &opener->entries[rindex];
            if(e->isLoaded){
                const char *ld = " LOADED";
                uint llen = strlen(ld);
                Graphics_PushText(state, x, ym, (char *)ld, llen, tcolSel, &pGlyph);
            }
        }
        
        
        if((int)i == list->active){
            Graphics_QuadPushBorder(state, 0, y0, lWidth, y1, 2, scol);
        }
#endif
        y0 += (1.0 + kViewSelectableListOffset) * scaling *
            state->font.fontMath.fontSizeAtRenderCall;
    }
    
    glUseProgram(font->cursorShader.id);
    
    Shader_UniformMatrix4(font->cursorShader, "projection", &state->projection.m);
    Shader_UniformMatrix4(font->cursorShader, "modelView", &state->scale.m);
    
    Graphics_QuadFlush(state);
    
    Graphics_PrepareTextRendering(state, &state->projection, &state->scale);
    Graphics_FlushText(state);
    
    Graphics_SetFontSize(state, currFontSize);
    glDisable(GL_SCISSOR_TEST);
    return 0;
}
