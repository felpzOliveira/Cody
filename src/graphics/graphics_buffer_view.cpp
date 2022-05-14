#include <graphics.h>
#include <glad/glad.h>
#include <gl3corefontstash.h>
#include <vector>
#include <app.h>
#include <file_provider.h>
#include <parallel.h>
#include <gitbase.h>
#include <iostream>
#include <dbgapp.h>

int OpenGLRenderLine(BufferView *view, OpenGLState *state,
                     Float &x, Float &y, uint lineNr);


static Float ComputeTransformOf(Float x, Transform *transform){
    vec3f p(x, 0, 0);
    p = transform->Point(p);
    return p.x;
}

void OpenGLRenderEmptyCursorRect(OpenGLBuffer *quad, vec2f lower,
                                 vec2f upper, vec4f color, uint thickness)
{
    Float x0 = lower.x, y0 = lower.y, x1 = upper.x, y1 = upper.y;
    if(quad->length + 24 > quad->size){
        Graphics_QuadFlush(quad);
    }
    uint w = thickness;
    Graphics_QuadPush(quad, vec2ui(x0+0, y0+0), vec2ui(x0+w, y1+0), color);
    Graphics_QuadPush(quad, vec2ui(x0+0, y1+0), vec2ui(x1+w, y1+w), color);
    Graphics_QuadPush(quad, vec2ui(x1+0, y1+0), vec2ui(x1+w, y0+w), color);
    Graphics_QuadPush(quad, vec2ui(x1+w, y0+w), vec2ui(x0+0, y0+0), color);
}

void OpenGLComputeCursorProjection(OpenGLState *state, OpenGLCursor *glCursor,
                                   Transform *transform, Float lineWidth,
                                   BufferView *view)
{
    if(glCursor->valid){
        Scroll *scroll = &view->scroll;
        Transform model = state->scale * scroll->horizontal;
        Float dif = 0;
        Float cursorMaxX = ComputeTransformOf(glCursor->pMax.x, &model);
        Float cursorMinX = ComputeTransformOf(glCursor->pMin.x, &model);

        if(cursorMaxX > lineWidth){
            cursorMaxX = ComputeTransformOf(glCursor->pMax.x, &state->scale);
            dif = cursorMaxX - lineWidth;
            scroll->horizontal = Translate(-dif, 0, 0);
        }else if(cursorMinX < 0){
            dif = ComputeTransformOf(glCursor->pMin.x, &state->scale);
            scroll->horizontal = Translate(-dif, 0, 0);
        }

        //TODO: I'm unsure how to reset the view, this seems to work for now
        if(glCursor->textPos.y == 0){
            scroll->horizontal = Transform();
        }

        *transform = scroll->horizontal;
    }
}

void OpenGLComputeCursor(OpenGLFont *font, OpenGLCursor *glCursor, const char *buffer,
                         uint len, Float x, Float cursorOffset, Float baseHeight)
{
    Float x0 = 0, x1 = 0, y0 = 0, y1 = 0;
    uint rawp = 0;
    int utf8Len = 1;
    int pUtf8Len = 1;
    char *ptr = (char *)buffer;
    glCursor->valid = 1;
    if(x > 0){
        rawp = StringComputeRawPosition(ptr, len, x-1, &utf8Len);
        x0 = fonsComputeStringAdvance(font->fsContext, ptr, rawp+utf8Len, &glCursor->pGlyph);
    }
    rawp = StringComputeRawPosition(ptr, len, x, &pUtf8Len);
    x1 = x0 + fonsComputeStringAdvance(font->fsContext, &ptr[rawp],
                                       pUtf8Len, &glCursor->pGlyph);

    y0 = 0;
    y1 = y0 + font->fontMath.fontSizeAtRenderCall;
    y0 += baseHeight;
    y1 += baseHeight;

    x0 += cursorOffset;
    x1 += cursorOffset;

    glCursor->pMin = vec2f(x0, y0);
    glCursor->pMax = vec2f(x1, y1);
}

#define CursorDefaultPadding 2
void OpenGLComputeCursor(OpenGLState *state, OpenGLCursor *glCursor,
                         Buffer *buffer, vec2ui cursor, Float cursorOffset,
                         Float baseHeight, vec2ui visibleLines)
{
    Float x0 = 0, x1 = 0, y0 = 0, y1 = 0;
    uint rawp = 0, rawpp = 0;
    int utf8Len = 1;
    int pUtf8Len = 1;
    char *ptr = buffer->data;

    glCursor->pGlyph = -1;
    glCursor->valid = 0;
    glCursor->textPos = cursor;

    if(cursor.x >= visibleLines.x && cursor.x <= visibleLines.y){
        glCursor->valid = 1;
        if(cursor.y > 0){
            rawp = Buffer_Utf8PositionToRawPosition(buffer, cursor.y-1, &utf8Len);
            x0 = fonsComputeStringAdvance(state->font.fsContext, ptr,
                                          rawp+utf8Len, &glCursor->pGlyph);
        }
        x0 = Max(0, x0-CursorDefaultPadding);

        rawp = Buffer_Utf8PositionToRawPosition(buffer, cursor.y, &pUtf8Len);
        rawpp = Buffer_PositionTabCompensation(buffer, rawp, 1);
        if(rawpp > rawp){
            // the tab compensation routine gives the next character after a tab
            // but for rendering cursor we need the rectangle untill the final tab
            // expansion value so we need to retract one
            rawpp -= 1;
            pUtf8Len += (rawpp - rawp);
        }

        x1 = x0 + fonsComputeStringAdvance(state->font.fsContext, &ptr[rawp],
                                           pUtf8Len, &glCursor->pGlyph) +
                  CursorDefaultPadding;

        y0 = (cursor.x - visibleLines.x) * state->font.fontMath.fontSizeAtRenderCall;
        y1 = y0 + state->font.fontMath.fontSizeAtRenderCall;
        y0 += baseHeight;
        y1 += baseHeight;

        x0 += cursorOffset;
        x1 += cursorOffset;

        glCursor->pMin = vec2f(x0, y0);
        glCursor->pMax = vec2f(x1, y1);
    }
}

void OpenGLRenderCursorSegment(OpenGLState *state, View *view, vec4f color, uint thickness){
    if(view->isActive && state->params.cursorSegments){
        OpenGLCursor *cursor = &state->glCursor;
        OpenGLCursor *gcursor = &state->glGhostCursor;
        Float y0 = 0, y1 = 0;
        // 1 - if the ghost cursor is visible than we can actually draw a line
        if(gcursor->valid){
            y0 = cursor->pMax.y;
            y1 = gcursor->pMax.y;
            // check if both cursors are on the same line
            // in this case we don't have anything to do
            if(IsZero(y0 - y1)) return;
            if(y0 > y1){
                y0 = gcursor->pMin.y;
                y1 = cursor->pMax.y;
            }else{
                y0 = cursor->pMin.y;
                y1 = gcursor->pMax.y;
            }
        }else{
            // 2 - if the ghost cursor is not visible we need
            //     to know if it up or down
            OpenGLFont *font = &state->font;
            BufferView *bView = View_GetBufferView(view);
            y1 = cursor->pMax.y;
            y0 = 0;
            if(view->descLocation == DescriptionTop){
                y0 = font->fontMath.fontSizeAtRenderCall;
            }

            vec2ui normal = BufferView_GetCursorPosition(bView);
            vec2ui ghost  = BufferView_GetGhostCursorPosition(bView);
            if(ghost.x > normal.x){ // our base cursor is up
                vec2ui vlines = BufferView_GetViewRange(bView);
                y0 = cursor->pMin.y;
                y1 = ((Float)ghost.x - (Float)vlines.x) * font->fontMath.fontSizeAtRenderCall;
            }
        }

        Graphics_QuadPush(state, vec2ui(0, y0), vec2ui(thickness, y1), color);
    }
}

void Graphics_RenderCursorAt(OpenGLBuffer *quad, Float x0, Float y0, Float x1,
                             Float y1, vec4f color, uint thickness, int isActive)
{
    if(isActive){
        if(appGlobalConfig.cStyle == CURSOR_RECT){
            Graphics_QuadPush(quad, vec2ui(x0, y0), vec2ui(x1, y1), color);
        }else if(appGlobalConfig.cStyle == CURSOR_DASH){
            x0 = x0 > 3 ? x0 - 3 : 0;
            OpenGLRenderEmptyCursorRect(quad, vec2f(x0, y0), vec2f(x0+3, y1),
                                        color, thickness);
            //Graphics_QuadPush(state, vec2ui(x0, y0), vec2ui(x0+3, y1), color);
        }else if(appGlobalConfig.cStyle == CURSOR_QUAD){
            Float thick = 3;
            y1 -= thick;
            Float x01 = x0 + thick;
            Float y01 = y1 + thick;
            Float y11 = y0 + thick;
            Float xh = x0 + (x1 - x0) * 0.6;
            Float xu = x0 + (x1 - x0) * 0.4;
            Graphics_QuadPush(quad, vec2ui(x0, y0), vec2ui(x01, y1), color);
            Graphics_QuadPush(quad, vec2ui(x0, y01), vec2ui(xh, y1), color);
            Graphics_QuadPush(quad, vec2ui(xu, y0), vec2ui(x1, y11), color);
            Graphics_QuadPush(quad, vec2ui(x1, y0), vec2ui(x1+thick, y01), color);
        }
    }else{
        if(appGlobalConfig.cStyle == CURSOR_RECT){
            OpenGLRenderEmptyCursorRect(quad, vec2f(x0, y0), vec2f(x1, y1),
                                        color, 2);
        }else if(appGlobalConfig.cStyle == CURSOR_DASH){
            x0 = x0 > 3 ? x0 - 3 : 0;
            OpenGLRenderEmptyCursorRect(quad, vec2f(x0, y0), vec2f(x0+3, y1),
                                        color, thickness);
        }else if(appGlobalConfig.cStyle == CURSOR_QUAD){
            Float thick = 3;
            y1 -= thick;
            Float x01 = x0 + thick;
            Float y01 = y1 + thick;
            Float y11 = y0 + thick;
            Float xh = x0 + (x1 - x0) * 0.6;
            Float xu = x0 + (x1 - x0) * 0.4;
            color.w *= 0.5;
            Graphics_QuadPush(quad, vec2ui(x0, y0), vec2ui(x01, y1), color);
            Graphics_QuadPush(quad, vec2ui(x0, y01), vec2ui(xh, y1), color);
            Graphics_QuadPush(quad, vec2ui(xu, y0), vec2ui(x1, y11), color);
            Graphics_QuadPush(quad, vec2ui(x1, y0), vec2ui(x1+thick, y01), color);
        }
    }
}

void OpenGLRenderCursor(OpenGLState *state, OpenGLCursor *glCursor, vec4f color,
                        uint thickness, int isActive)
{
    Float x0 = glCursor->pMin.x, x1 = glCursor->pMax.x;
    Float y0 = glCursor->pMin.y, y1 = glCursor->pMax.y;
    OpenGLBuffer *quad = &state->glQuadBuffer;
    Graphics_RenderCursorAt(quad, x0, y0, x1, y1, color, thickness, isActive);
}

static void OpenGLRenderLineNumber(BufferView *view, OpenGLFont *font,
                                   Float &x, Float &y, uint lineNr, char linen[32],
                                   Theme *theme)
{
    if(view->renderLineNbs){
        int previousGlyph = -1;
        vec2ui cursor = BufferView_GetCursorPosition(view);
        int spacing = DigitCount(BufferView_GetLineCount(view));
        int ncount = DigitCount(lineNr+1);
        memset(linen, ' ', spacing-ncount);
        int k = snprintf(&linen[spacing-ncount], 32, "%u", lineNr+1);
        linen[spacing-ncount + k] = 0;
        vec4i col = GetUIColor(theme, UILineNumbers);
        if(lineNr == cursor.x){
            col = GetUIColor(theme, UILineNumberHighlighted);
        }

        if(!view->isActive){
            col.w *= 0.2;
        }

        x = fonsStashMultiTextColor(font->fsContext, x, y, col.ToUnsigned(),
                                    linen, NULL, &previousGlyph);
    }
}

void OpenGLRenderAllLineNumbers(OpenGLState *state, BufferView *view, Theme *theme){
    if(view->renderLineNbs){
        Float x0 = 0;
        Float x = x0;
        Float y = 0;
        char linen[32];
        OpenGLFont *font = &state->font;
        vec2ui lines = BufferView_GetViewRange(view);

        fonsClearState(font->fsContext);
        fonsSetSize(font->fsContext, font->fontMath.fontSizeAtRenderCall);
        fonsSetAlign(font->fsContext, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

        glUseProgram(font->shader.id);
        Shader_UniformMatrix4(font->shader, "projection", &state->projection.m);
        Shader_UniformMatrix4(font->shader, "modelView", &state->scale.m);

        uint st0 = lines.x;
        uint ed0 = lines.y;
        if(lines.x > 0){
            st0 -= 1;
            y -= font->fontMath.fontSizeAtRenderCall;
        }

        if(lines.y < view->lineBuffer->lineCount-1){
            ed0 += 1;
        }

        for(uint i = st0; i < ed0; i++){
            OpenGLRenderLineNumber(view, font, x, y, i, linen, theme);
            x = x0;
            y += font->fontMath.fontSizeAtRenderCall;
        }

        fonsStashFlush(font->fsContext);
    }
}

void _Graphics_RenderTextBlock(OpenGLState *state, BufferView *view, Float baseHeight,
                               Transform *projection, Transform *model, vec2ui lines)
{
    Float x0 = 2.0f;
    Float x = x0;
    Float y = baseHeight;
    OpenGLFont *font = &state->font;

    Graphics_PrepareTextRendering(state, projection, &state->model);

    x = x0;
    y = baseHeight;
    uint st0 = lines.x;
    uint ed0 = lines.y;

    if(lines.x > 0){
        st0 -= 1;
        y -= font->fontMath.fontSizeAtRenderCall;
    }

    if(lines.y < view->lineBuffer->lineCount-1){
        ed0 += 1;
    }

    for(uint i = st0; i < ed0; i++){
        if(OpenGLRenderLine(view, state, x, y, i)){
#if 0
            vec4i s(128);
            int pGlyph  = -1;
            const char *p = "..";
            uint plen = strlen(p);
            Float xof = fonsComputeStringAdvance(font->fsContext, p, plen, &pGlyph);
            Float xst = state->renderLineWidth - view->lineOffset - xof - 2;
            Float yi = y - font->fontMath.fontSizeAtRenderCall;
            pGlyph  = -1;
            fonsStashMultiTextColor(font->fsContext, xst, yi, s.ToUnsigned(),
                                    p, NULL, &pGlyph);
#endif
        }
        x = x0;
        y += font->fontMath.fontSizeAtRenderCall;
    }

    Graphics_FlushText(state);
}

void _Graphics_RenderDbgElements(OpenGLState *state, View *view, Float lineSpan,
                                 Transform *model, Transform *projection)
{
    int pGlyph = -1;
    OpenGLFont *font = &state->font;
    BufferView *bufferView = View_GetBufferView(view);
    const char *dbgChar = "◄";
    int n = strlen(dbgChar);
    vec4i col = GetUIColor(defaultTheme, UIDbgArrowColor);
    Geometry *geometry = &view->geometry;

    std::optional<LineHints> c = BufferView_GetLineHighlight(bufferView);
    if(c){
        LineHints cx = c.value();
        vec2ui visibleLines = BufferView_GetViewRange(bufferView);
        vec2f y = Graphics_GetLineYPos(state, visibleLines, cx.line, view);
        if(y.x < 0 || y.y < 0){
            // it is actually off-screen but because we render +- n lines
            // we can't actually compute it.
            return;
        }
        vec2ui u = ScreenToGL(geometry->upper - geometry->lower, state);
        vec2ui a1(lineSpan, u.y + font->fontMath.fontSizeAtRenderCall);
        Float w = fonsComputeStringAdvance(font->fsContext, dbgChar, n, &pGlyph);

        Float x = a1.x - 2.0 * w;

        glUseProgram(font->cursorShader.id);
        vec4f colf = GetUIColorf(defaultTheme, UIDbgLineHighlightColor);
        if(cx.state == LHINT_STATE_UNKNOW){
            // TODO: Just don't.
            col = vec4i(255, 0, 0, 255);
        }else if(cx.state == LHINT_STATE_INTERRUPT){
            // TODO: JUST DON'T.
            colf = vec4f(0.2, 0., 0., 1.0);
        }

        Shader_UniformMatrix4(font->cursorShader, "projection", &projection->m);
        Graphics_QuadPush(state, vec2ui(0, y.x), vec2ui(lineSpan, y.y), colf);

        Shader_UniformMatrix4(font->cursorShader, "modelView", &state->scale.m);
        Graphics_QuadFlush(state, 1);
        pGlyph = -1;
        Graphics_PrepareTextRendering(state, projection, &state->model);

        Graphics_PushText(state, x, y.x, (char *)dbgChar, n, col, &pGlyph);
        Graphics_FlushText(state);
    }
}

void _Graphics_RenderCursorElements(OpenGLState *state, View *view, Float lineSpan,
                                    Transform *model, Transform *projection)
{
    int pGlyph = -1;
    BufferView *bufferView = View_GetBufferView(view);
    ViewState vstate = View_GetState(view);

    OpenGLFont *font = &state->font;
    OpenGLCursor *cursor = &state->glCursor;
    Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor->textPos.x);

    if(!buffer){
        // This is a bug
        BUG();
        BUG_PRINT("Warning: Attempting to render cursor at position {%d %d} "
                  "but linebuffer has {%d}\n", cursor->textPos.x, cursor->textPos.y,
                  bufferView->lineBuffer->lineCount);
        cursor->textPos.x = Max(0, bufferView->lineBuffer->lineCount-1);
        buffer = BufferView_GetBufferAt(bufferView, cursor->textPos.x);
        if(!buffer){
            BUG_PRINT("Error: Bufferview is inconsistent, not locked?\n");
            return;
        }
    }

    if(vstate == View_FreeTyping){
        vec4f col = Graphics_GetCursorColor(bufferView, defaultTheme);
        vec4f segCol(1.0,1.0,1.0,0.25);
        uint dashThick = 4;
        if(CurrentThemeIsLight()){
            segCol = vec4f(0.3, 0.3, 0.3, 0.25);
        }
        glUseProgram(font->cursorShader.id);
        Shader_UniformMatrix4(font->cursorShader, "projection", &projection->m);
        Shader_UniformMatrix4(font->cursorShader, "modelView", &model->m);

        glDisable(GL_BLEND);
        OpenGLRenderCursorSegment(state, view, segCol, 8.0);

        OpenGLRenderCursor(state, &state->glCursor, col,
                           dashThick, bufferView->isActive && vstate == View_FreeTyping);

        if(state->glGhostCursor.valid){
            col = Graphics_GetCursorColor(bufferView, defaultTheme, 1);
            OpenGLRenderCursor(state, &state->glGhostCursor, col, dashThick, 0);
        }
    }

    if(bufferView->isActive && vstate == View_FreeTyping){
        /* Redraw whatever character we are on top of */
        // TODO: Why do we need to flush the quad buffer here in order to render
        //       the last character?
        Graphics_QuadFlush(state);
        if(cursor->textPos.y < buffer->count && appGlobalConfig.cStyle == CURSOR_RECT){
            vec2f p = state->glCursor.pMin;
            pGlyph = state->glCursor.pGlyph;
            int len = 0;
            uint rawp = Buffer_Utf8PositionToRawPosition(buffer, cursor->textPos.y, &len);
            char *chr = &buffer->data[rawp];
            if(*chr != '\t'){
                Graphics_PrepareTextRendering(state, projection, &state->model);
                vec4i cc = GetUIColor(defaultTheme, UICharOverCursor);
                fonsStashMultiTextColor(font->fsContext, p.x, p.y, cc.ToUnsigned(),
                                        chr, chr+len, &pGlyph);
                Graphics_FlushText(state);
            }
        }
    }else{
        Graphics_QuadFlush(state);
    }
}

void Graphics_RenderTextBlock(OpenGLState *state, BufferView *bufferView,
                              Float baseHeight, Transform *projection)
{
    vec2ui lines;
    lines = BufferView_GetViewRange(bufferView);
    _Graphics_RenderTextBlock(state, bufferView, baseHeight,
                              projection, &state->model, lines);
}

void Graphics_RenderCursorElements(OpenGLState *state, View *view,
                                   Float lineSpan, Transform *projection)
{
    _Graphics_RenderCursorElements(state, view, lineSpan, &state->model, projection);
}

void Graphics_RenderScopeSections(OpenGLState *state, View *vview, Float lineSpan,
                                  Transform *projection, Transform *model, Theme *theme)
{
    BufferView *view = View_GetBufferView(vview);
    if(view->isActive){
        OpenGLFont *font = &state->font;
        vec2ui cursor = BufferView_GetCursorPosition(view);
        vec2ui visibleLines = BufferView_GetViewRange(view);

        vec4f linesColor = GetUIColorf(theme, UIScopeLine);

        glUseProgram(font->cursorShader.id);
        Shader_UniformMatrix4(font->cursorShader, "projection", &projection->m);

        if(BufferView_CursorNestIsValid(view)){
            NestPoint *start = view->startNest;
            NestPoint *end   = view->endNest;
            Float minY = -font->fontMath.fontSizeAtRenderCall;
            Float maxY = (view->sController.currentMaxRange + 1)*
                font->fontMath.fontSizeAtRenderCall;
            struct _Quad{
                vec2ui left, right;
                vec4f color;
                int depth;
            };

            std::vector<_Quad> quads;

            if(vview->descLocation == DescriptionTop){
                minY = font->fontMath.fontSizeAtRenderCall;
                maxY += font->fontMath.fontSizeAtRenderCall;
            }

            uint it = view->startNestCount;
            for(uint f = 0; f < it; f++){
                NestPoint v = start[f];
                if(v.valid && v.id == TOKEN_ID_BRACE_OPEN){ // valid
                    NestPoint e = end[v.comp];
                    Buffer *bStart = BufferView_GetBufferAt(view, v.position.x);
                    Buffer *bEnd   = BufferView_GetBufferAt(view, e.position.x);

                    uint l0 = AppComputeLineIndentLevel(bStart);
                    uint l1 = AppComputeLineIndentLevel(bEnd);

                    // only do it for aligned stuff
                    if(l0 == l1 && e.position.x > v.position.x + 1){
                        // check if their non empty token match position
                        vec2ui p0(0, 0);
                        vec2ui p1(0, 0);
                        uint n = Max(bStart->tokenCount, bEnd->tokenCount);
                        for(uint i = 0; i < n; i++){
                            if(i < bStart->tokenCount && p0.y == 0){
                                Token *t = &bStart->tokens[i];
                                if(t->identifier != TOKEN_ID_SPACE){
                                    p0.x = t->position;
                                    p0.y = 1;
                                }
                            }

                            if(i < bEnd->tokenCount && p1.y == 0){
                                Token *t = &bEnd->tokens[i];
                                if(t->identifier != TOKEN_ID_SPACE){
                                    p1.x = t->position;
                                    p1.y = 1;
                                }
                            }

                            if(p1.y != 0 && p0.y != 0) break;
                        }

                        if((p1.y == 0 || p0.y == 0) || (p1.x != p0.x)){
                            continue;
                        }

                        // looks like this thing needs rendering
                        Float x0 = 0, x1 = 0, y0 = 0, y1 = 0;
                        int pGlyph = -1;
                        char *p = &bStart->data[0];

                        //TODO: UTF-8
                        x0 = fonsComputeStringAdvance(font->fsContext, p, p0.x, &pGlyph);
                        x1 = fonsComputeStringAdvance(font->fsContext,
                                                      &p[p0.x], 1, &pGlyph);

                        Float topX = x0 + 0.5f * x1;
                        Float botX = topX;
                        y0 = ((Float)v.position.x - (Float)visibleLines.x + 1) *
                            font->fontMath.fontSizeAtRenderCall;

                        y1 = y0 + (e.position.x - v.position.x - 1) *
                            font->fontMath.fontSizeAtRenderCall;

                        if(vview->descLocation == DescriptionTop){
                            y1 += font->fontMath.fontSizeAtRenderCall;
                            y0 += font->fontMath.fontSizeAtRenderCall;
                        }

                        y0 = Max(y0, minY);
                        y1 = Min(y1, maxY);

                        Graphics_LinePush(state, vec2ui(topX, y0), vec2ui(botX, y1),
                                          linesColor);
                    }

                    // render the quads no matter what
                    Float qy0 = 0, qy1 = 0, qx0 = 0, qx1 = 0;
                    qy0 = ((Float)v.position.x - (Float)visibleLines.x) *
                        font->fontMath.fontSizeAtRenderCall;

                    qy1 = qy0 + (e.position.x - v.position.x + 1) *
                        font->fontMath.fontSizeAtRenderCall;

                    if(vview->descLocation == DescriptionTop){
                        qy0 += font->fontMath.fontSizeAtRenderCall;
                        qy1 += font->fontMath.fontSizeAtRenderCall;
                    }

                    //TODO: Review this, this adds spacing for initialization of the scoped
                    //      quads
                    //qx0 = view->lineOffset;
                    qx1 = qx0 + lineSpan;

                    qy0 = Max(qy0, minY);
                    qy1 = Min(qy1, maxY);

                    vec4f color = GetNestColorf(theme, TOKEN_ID_SCOPE, v.depth);
                    quads.push_back({
                                        .left = vec2ui(qx0, qy0),
                                        .right = vec2ui(qx1, qy1),
                                        .color = color,
                                        .depth = v.depth
                                    });
                }
            }

            int quadLen = quads.size();
            short n = GetBackTextCount(theme);

            if(quadLen > 0 && n == 1){
                _Quad quad = quads.at(quadLen-1);
                Graphics_QuadPush(state, quad.left, quad.right,
                                  quad.color);
            }else{
                for(int i = quadLen - 1; i >= 0; i--){
                    _Quad quad = quads.at(i);
                    Graphics_QuadPush(state, quad.left, quad.right,
                                      quad.color);
                }
            }

            Shader_UniformMatrix4(font->cursorShader, "modelView", &state->scale.m);
            Graphics_QuadFlush(state, 0);
        }

        // Only render the line highlight if the mouse has not selected a range
        if(!BufferView_GetRangeVisible(view)){
            Shader_UniformMatrix4(font->cursorShader, "modelView", &state->scale.m);
            vec4f col = GetUIColorf(theme, UICursorLineHighlight);
            Float cy0 = ((Float)cursor.x - (Float)visibleLines.x) *
                            font->fontMath.fontSizeAtRenderCall;

            if(vview->descLocation == DescriptionTop){
                cy0 += font->fontMath.fontSizeAtRenderCall;
            }

            Float cy1 = cy0 + font->fontMath.fontSizeAtRenderCall;
            Graphics_QuadPush(state, vec2ui(0, cy0),
            vec2ui(lineSpan, cy1), col);

            Float w = GetLineBorderWidth(theme);
            if(w > 0){
                vec4f cc(1);
                Graphics_QuadPush(state, vec2ui(0, cy0), vec2ui(w, cy1), cc);
                Graphics_QuadPush(state, vec2ui(0, cy1-w), vec2ui(lineSpan, cy1), cc);
                Graphics_QuadPush(state, vec2ui(0, cy0), vec2ui(lineSpan, cy0+w), cc);
                Graphics_QuadPush(state, vec2ui(lineSpan-w, cy0), vec2ui(lineSpan, cy1), cc);
            }
        }

        Graphics_QuadFlush(state, 0);

        Shader_UniformMatrix4(font->cursorShader, "modelView", &model->m);
        Graphics_LineFlush(state);

    }
}

void Graphics_RenderSpacesHighlight(OpenGLState *state, View *vview, Transform *projection,
                                    Theme *theme)
{
    BufferView *view = View_GetBufferView(vview);
    if(view->isActive){
        LineBuffer *lineBuffer=  BufferView_GetLineBuffer(view);
        if(lineBuffer){
            OpenGLFont *font = &state->font;
            int previousGlyph = -1;
            vec2ui visibleLines = BufferView_GetViewRange(view);
            vec4f emptyColor(0.5, 0.5, 0.1, 0.5);
            //vec4f extraColor(0.4, 0.1, 0.1, 0.5);

            glUseProgram(font->cursorShader.id);
            Shader_UniformMatrix4(font->cursorShader, "projection", &projection->m);
            Shader_UniformMatrix4(font->cursorShader, "modelView", &state->scale.m);
            for(uint i = visibleLines.x; i <= visibleLines.y; i++){
                Buffer *buffer = LineBuffer_GetBufferAt(lineBuffer, i);
                if(!buffer) continue;

                vec2f y = Graphics_GetLineYPos(state, visibleLines, i, vview);
                vec2f x(2.0f, 2.0f);

                if(Buffer_IsBlank(buffer) && buffer->tokenCount == 1){
                    Token *token = &buffer->tokens[0];
                    uint pos = 0;
                    while((int)pos < token->size){
                        x.y += fonsComputeStringAdvance(font->fsContext, " ", 1,
                                                        &previousGlyph);
                        pos++;
                    }

                    Graphics_QuadPush(state, vec2f(x.x, y.x), vec2f(x.y, y.y), emptyColor);
                }else if(buffer->tokenCount > 1){
                    int previousGlyph = -1;
                    Token *token = &buffer->tokens[buffer->tokenCount-1];
                    if(token->identifier == TOKEN_ID_SPACE){
                        x.x += Graphics_GetTokenXPos(state, buffer,
                                        buffer->tokenCount-1, previousGlyph);
                        x.y = x.x + Graphics_GetTokenXSize(state, buffer,
                                        buffer->tokenCount-1, previousGlyph);

                        Graphics_QuadPush(state, vec2f(x.x, y.x),
                                          vec2f(x.y, y.y), emptyColor);
                    }
                }
            }

            Graphics_QuadFlush(state);
        }
    }
}

static void Graphics_RenderDbgBreaks(OpenGLState *state, View *vview, Transform *projection,
                                     Float lineSpan, Theme *theme)
{
    BufferView *view = View_GetBufferView(vview);
    LineBuffer *lBuffer = BufferView_GetLineBuffer(view);
    if(view->isActive && Dbg_IsRunning() && lBuffer){
        OpenGLFont *font = &state->font;
        vec2ui visibleLines = BufferView_GetViewRange(view);
        vec4f col(0.3, 0.3, 0.05, 0.3);
        vec4i cc(255, 0, 0, 255);
        const char *dbgChar = "●";
        int n = strlen(dbgChar);
        int pGlyph = -1;

        if(CurrentThemeIsLight()){
            col = GetUIColorf(theme, UICursorLineHighlight);
            cc = vec4i(200, 0, 0, 255);
            col.w = 0.3;
        }

        Graphics_PrepareTextRendering(state, projection, &state->model);

        auto fn = [&](DbgBkpt *bkp) -> void{
            uint line = (uint)(bkp->line > 0 ? bkp->line-1 : 0);
            if(bkp->enabled && line >= visibleLines.x && line <= visibleLines.y){
                pGlyph = -1;
                vec2f y = Graphics_GetLineYPos(state, visibleLines, line, vview);
                //Graphics_QuadPushBorder(state, 2.0f, y.x, lineSpan, y.y, 2.0, col);
                Graphics_QuadPush(state, vec2f(2.0f, y.x), vec2f(lineSpan, y.y), col);
                Float x = 2.0f;
                Float yx = y.x;
                Graphics_PushText(state, x, yx, (char *)dbgChar, n, cc, &pGlyph);
            }
        };
        DbgSupport_ForEachBkpt(LineBuffer_GetStoragePath(lBuffer), fn);

        Graphics_FlushText(state);

        glUseProgram(font->cursorShader.id);
        Shader_UniformMatrix4(font->cursorShader, "projection", &projection->m);
        Shader_UniformMatrix4(font->cursorShader, "modelView", &state->scale.m);
        Graphics_QuadFlush(state);
    }
}

static vec4f Graphics_GetLineHighlightColor(uint utypes){
    LineTypes type = (LineTypes)utypes;
    switch(type){
        case GIT_LINE_INSERTED: return vec4f(0.1,0.4,0.1,0.5);
        case GIT_LINE_REMOVED: return vec4f(0.5,0.1,0.1,0.5);
        default: return vec4f(0.8, 0.0, 0.8, 1.0); // bug, paint it pink
    }
}

void Graphics_RenderBuildErrors(OpenGLState *state, View *view, Float lineSpan,
                                Transform *projection, Transform *model, Theme *theme)
{
    BufferView *bufferView = View_GetBufferView(view);
    LineBuffer *lineBuffer = BufferView_GetLineBuffer(bufferView);
    OpenGLFont *font = &state->font;
    if(!lineBuffer) return;

    char *str = LineBuffer_GetStoragePath(lineBuffer);
    std::string path(str);
    if(path.size() == 0) return;

    vec2ui visibleLines = BufferView_GetViewRange(bufferView);
    vec4f col_err(0.8, 0.0, 0.0, 0.25);
    vec4f col_war(0.8, 0.8, 0.2, 0.25);
    vec4f col_over_errf(0.8, 0.0, 0.0, 1.0);
    vec4f col_over_warf(0.8, 0.8, 0.2, 1.0);
    vec4i col_over_err = ColorFromRGBA(col_over_errf);
    vec4i col_over_war = ColorFromRGBA(col_over_warf);

    Float currFontSize = state->font.fontMath.fontSizeAtDisplay;
    Float fontSize = currFontSize * 0.85;
    Float scale = currFontSize / fontSize;
    std::vector<vec3f> render_pts;
    int counter = 0;

    auto compute_render_pts = [&](BuildError &err) -> void{
        int pGlyph = -1;
        if(path != err.file) return;

        uint line = (uint)err.line > 0 ? err.line - 1 : 0;
        if(visibleLines.x > line || visibleLines.y < line) return;
        Buffer *buf = LineBuffer_GetBufferAt(lineBuffer, line);
        std::string str(buf->data, buf->taken);
        str += " ";
        Float x = fonsComputeStringAdvance(font->fsContext, (char *)str.c_str(),
                                           str.size(), &pGlyph);

        vec2f y = Graphics_GetLineYPos(state, visibleLines, line, view);
        render_pts.push_back(vec3f(x, y.x, y.y));
    };

    auto render_lines_rects = [&](BuildError &err) -> void{
        int pGlyph = -1;
        if(path != err.file) return;

        uint line = (uint)err.line > 0 ? err.line - 1 : 0;
        if(visibleLines.x > line || visibleLines.y < line) return;
        vec3f p = render_pts[counter++];
        vec2f y = vec2f(p.y, p.z);
        Float x = p.x * scale;
        Float yx = y.x * scale + fontSize * 0.25;

        vec4f col = col_war;
        vec4i tcol = col_over_war;
        if(err.is_error){
            col = col_err;
            tcol = col_over_err;
        }

        int where = 0, start = 0;
        for(int i = 0; i < (int)err.message.size(); i++){
            if(err.message[i] == '\n'){
                break;
            }
            where = i;
        }
        std::string msg = err.message.substr(0, where+1);
        if(StringStartsWith((char *)msg.c_str(), msg.size(), (char *)": ", 2)){
            start = 2;
        }
        // Search next/previous
        const char *pp = msg.c_str();
        Graphics_PushText(state, x, yx, (char *)"- ", 2, tcol, &pGlyph);
        Graphics_PushText(state, x, yx, (char *)&pp[start],
                          msg.size()-start, tcol, &pGlyph);

        Graphics_QuadPush(state, vec2f(2.0f, y.x), vec2f(lineSpan, y.y), col);
    };

    for(BuildError &err : state->bErrors){
        compute_render_pts(err);
    }

    Graphics_SetFontSize(state, fontSize);
    Graphics_PrepareTextRendering(state, projection, &state->scale);

    for(BuildError &err : state->bErrors){
        render_lines_rects(err);
    }

    Graphics_FlushText(state);

    Graphics_SetFontSize(state, currFontSize);

    glUseProgram(font->cursorShader.id);
    Shader_UniformMatrix4(font->cursorShader, "projection", &projection->m);
    Shader_UniformMatrix4(font->cursorShader, "modelView", &model->m);
    Graphics_QuadFlush(state, 1);
}

bool Graphics_RenderLineHighlight(OpenGLState *state, View *vview, Float lineSpan,
                                  Transform *projection, Transform *model, Theme *theme)
{
    BufferView *view = View_GetBufferView(vview);
    OpenGLFont *font = &state->font;
    bool rendered = false;
    if(view->isActive){
        LineBuffer *lineBuffer = BufferView_GetLineBuffer(view);
        bool any = false;
        if(lineBuffer){
            ViewType vtype = BufferView_GetViewType(view);
            vec2ui visibleLines = BufferView_GetViewRange(view);
            // TODO: Make this better
            if(vtype == DbgView && DbgApp_IsStopped()){
                _Graphics_RenderDbgElements(state, vview, lineSpan, model, projection);
                return true;
            }

            std::vector<LineHighlightInfo> *lh =
                                        LineBuffer_GetLineHighlightPtr(lineBuffer, 0);
            std::vector<vec2ui> *ptr = LineBuffer_GetDiffRangePtr(lineBuffer, &any);
            if(!(!any || !ptr || lh->size() < 1)){
                glUseProgram(font->cursorShader.id);
                Shader_UniformMatrix4(font->cursorShader, "projection", &projection->m);
                Shader_UniformMatrix4(font->cursorShader, "modelView", &state->scale.m);
                rendered = true;
                for(vec2ui val : *ptr){
                    if(!(val.x >= visibleLines.x && val.x <= visibleLines.y)){
                        continue;
                    }

                    Float y0 = ((Float)val.x - (Float)visibleLines.x) *
                    font->fontMath.fontSizeAtRenderCall;
                    if(vview->descLocation == DescriptionTop){
                        y0 += font->fontMath.fontSizeAtRenderCall;
                    }

                    Float y1 = y0 + font->fontMath.fontSizeAtRenderCall;
                    Graphics_QuadPush(state, vec2ui(0, y0), vec2ui(lineSpan, y1),
                    Graphics_GetLineHighlightColor(val.y));
                }

                Graphics_QuadFlush(state);

                Shader_UniformMatrix4(font->cursorShader, "modelView", &model->m);
                Graphics_LineFlush(state);
            }
        }

        if(BufferView_GetRangeVisible(view)){
            OpenGLCursor *gstart = &state->glCursor;
            OpenGLCursor *gend   = &state->glGhostCursor;
            vec2ui cursor = BufferView_GetCursorPosition(view);
            vec2ui ghostCursor = BufferView_GetGhostCursorPosition(view);
            vec2ui vlines = BufferView_GetViewRange(view);
            vec2ui start = cursor, end = ghostCursor;
            glUseProgram(font->cursorShader.id);
            Shader_UniformMatrix4(font->cursorShader, "projection", &projection->m);
            Shader_UniformMatrix4(font->cursorShader, "modelView", &state->scale.m);
            if(start.x > end.x || (start.x == end.x && start.y > end.y)){
                start  = ghostCursor;
                end    = cursor;
                gstart = &state->glGhostCursor;
                gend   = &state->glCursor;
            }

            if(start.x < vlines.x) start.x = vlines.x;
            if(end.x > vlines.y) end.x = vlines.y;

            Float xpos = gstart->pMin.x;
            Float ypos = gstart->pMin.y;
            Float ey = 0;
            vec4f cc = GetUIColorf(theme, UISelectionColor);
            for(uint i = start.x; i < end.x; i++){
                // these lines need to be painted at full
                ey = ypos + font->fontMath.fontSizeAtRenderCall;
                Graphics_QuadPush(state, vec2ui(xpos, ypos),
                                  vec2ui(xpos + lineSpan, ey), cc);
                ypos += font->fontMath.fontSizeAtRenderCall;
                xpos = 0;
            }

            ey = ypos + font->fontMath.fontSizeAtRenderCall;
            Float ex = gend->pMin.x;
            Graphics_QuadPush(state, vec2ui(xpos, ypos), vec2ui(ex, ey), cc);
            Graphics_QuadFlush(state, 1);
        }
    }

    return rendered;
}

int OpenGLRenderLine(BufferView *view, OpenGLState *state,
                     Float &x, Float &y, uint lineNr)
{

    int previousGlyph = -1;
    int largeLine = 0;
    OpenGLFont *font = &state->font;
    Tokenizer *tokenizer = FileProvider_GetLineBufferTokenizer(view->lineBuffer);
    SymbolTable *symTable = tokenizer->symbolTable;
    Buffer *buffer = BufferView_GetBufferAt(view, lineNr);

    vec4i col;
    if(!buffer) return largeLine;

    //TODO: This is where we would wrap?
    if(buffer->taken > 0){
        uint pos = 0;
        for(uint i = 0; i < buffer->tokenCount; i++){
            Token *token = &buffer->tokens[i];
            while((int)pos < token->position){
                char v = buffer->data[pos];
                if(v == '\r' || v == '\n'){
                    pos++;
                    continue;
                }

                if(v == '\t' || v == ' '){
                    x += fonsComputeStringAdvance(font->fsContext, " ", 1,
                                                  &previousGlyph);
                    pos ++;
                    continue;
                }

                if(v == 0){
                    return 0;
                }

                printf("Position %u  ==> Token %u, char \'%c\'\n", pos, token->position, v);
                printf("Data : %s, taken: %u\n", buffer->data, buffer->taken);
                return 0;
                AssertA(0, "Invalid character");
            }

            if(token->identifier == TOKEN_ID_SPACE){
                // NOTE: nothing to do, space was already inserted
            }else if(token->position < (int)buffer->taken){
                char *p = &buffer->data[token->position];
                char *e = &buffer->data[token->position + token->size];
                int r = Graphics_ComputeTokenColor(p, token, symTable, defaultTheme,
                                                   lineNr, i, view, &col);
                if(r < 0) continue;
                pos += token->size;
                x = fonsStashMultiTextColor(font->fsContext, x, y, col.ToUnsigned(),
                                            p, e, &previousGlyph);
                if(x > state->renderLineWidth - view->lineOffset){
                    largeLine = 1;
                }
            }
        }
    }

    return largeLine;
}


void Graphics_RenderFrame(OpenGLState *state, View *vview,
                          Transform *projection, Float lineSpan, Theme *theme)
{
    BufferView *view = View_GetBufferView(vview);
    OpenGLFont *font = &state->font;
    Geometry *geometry = &view->geometry;

    int dummyGlyph = -1;
    vec2ui l = vec2ui(0, 0);
    vec2ui u = ScreenToGL(geometry->upper - geometry->lower, state);

    // Render the file description of this buffer view
    // TODO: states
    char desc[256];
    char enddesc[256];
    enddesc[0] = 0;
    Float pc = BufferView_GetDescription(view, desc, sizeof(desc),
                                         enddesc, sizeof(enddesc));

    glDisable(GL_BLEND);
    glUseProgram(font->cursorShader.id);
    Shader_UniformMatrix4(font->cursorShader, "projection", &projection->m);
    Shader_UniformMatrix4(font->cursorShader, "modelView", &state->scale.m);

    vec2ui a1(u.x, u.y + font->fontMath.fontSizeAtRenderCall);
    vec2ui a0(l.x, u.y - font->fontMath.fontSizeAtRenderCall);

    vec4f col = GetUIColorf(theme, UIBackground);

    if(vview->descLocation == DescriptionTop){
        a0 = vec2ui(l.x, l.y);
        a1 = vec2ui(u.x, l.y + font->fontMath.fontSizeAtRenderCall);
    }else if(vview->descLocation != DescriptionBottom){
        AssertA(0, "Invalid description position");
    }

    col.w = 1; // make sure this thing does not suffer blending from behind text
    Graphics_QuadPush(state, a0, a1, col);

    //col = vec3f(1.8 * g, 2 * g, g);
    vec4f cc = GetUIColorf(theme, UIScrollBarColor);
    vec2f b0 = vec2f((Float)a0.x + (Float)(a1.x - a0.x) * pc, (Float)a0.y);
    Graphics_QuadPush(state, vec2ui((uint)b0.x, (uint)b0.y), a1, cc);

    Graphics_QuadFlush(state);

    //TODO: Wut is happening, this is broken, aspect ratio gets f***.
#if 0
    uint texId = Graphics_FetchTextureFor(state, ext, &offset);
    int offset = 0;
    LineBuffer *lineBuffer = View_GetBufferViewLineBuffer(vview);
    FileExtension ext = LineBuffer_GetExtension(lineBuffer);
    if(texId > 0){
        int size = 80;
        size -= offset;
        vec2ui p(a1.x - size, b0.y);
        printf("Rendering at: %u %u\n", p.x, p.y);
        int needs_render = Graphics_ImagePush(state, p, p+size, texId);
        if(needs_render){
            Graphics_ImageFlush(state);
            Graphics_ImagePush(state, p, p+size, texId);
        }

        Graphics_ImageFlush(state);
    }
#endif

    fonsClearState(font->fsContext);
    fonsSetSize(font->fsContext, font->fontMath.fontSizeAtRenderCall);
    fonsSetAlign(font->fsContext, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(font->shader.id);
    Shader_UniformMatrix4(font->shader, "projection", &projection->m);
    Shader_UniformMatrix4(font->shader, "modelView", &state->scale.m);

    Float x = a0.x;
    Float y = a0.y;
    vec4i c = GetColor(theme, TOKEN_ID_DATATYPE);
    fonsStashMultiTextColor(font->fsContext, x, y, c.ToUnsigned(),
                            desc, NULL, &dummyGlyph);
    int n = strlen(enddesc);
    Float w = 0;
    if(n > 0){
        w = fonsComputeStringAdvance(font->fsContext, enddesc, n, &dummyGlyph);
        fonsStashMultiTextColor(font->fsContext, a1.x - 1.5 * w, y, c.ToUnsigned(),
                                enddesc, NULL, &dummyGlyph);
    }

    fonsStashFlush(font->fsContext);

    uint currFontSize = font->fontMath.fontSizeAtDisplay;
    uint fSize = currFontSize > 5 ? currFontSize - 5 : currFontSize;
    Float scale = font->fontMath.invReduceScale;

    Graphics_SetFontSize(state, fSize, fSize + FONT_UPSCALE_DEFAULT_OFFSET);
    Graphics_PrepareTextRendering(state, projection, &state->scale);

    int is_tab = 0;
    int tabSpace = AppGetTabConfiguration(&is_tab);
    enddesc[0] = 0;
    std::string head, fmt;
    int k = 0, kk = 0;

    if(is_tab){
        k = snprintf(enddesc, sizeof(enddesc), " TAB - %d", tabSpace);
    }else{
        k = snprintf(enddesc, sizeof(enddesc), " SPACE - %d", tabSpace);
    }

    if(Git_GetReferenceHeadName(head)){
        kk = snprintf(&enddesc[k], sizeof(enddesc) - k, " %s", head.c_str());
        k += kk;
    }

    Float f = fonsComputeStringAdvance(font->fsContext, enddesc, k, &dummyGlyph);
    Float rScale = font->fontMath.invReduceScale / scale;
    w = w * rScale;
    vec2f half = (vec2f(geometry->upper) - vec2f(geometry->lower)) *
                        font->fontMath.invReduceScale * 0.5;
    x = 2.0 * half.x;
    if(n > 0){
        x -= (1.5 * w + 1.2 * f);
    }else{
        x -= 1.2 * f;
    }

    y = (a1.y + a0.y) * 0.5 - 0.25 * font->fontMath.fontSizeAtRenderCall;
    Graphics_PushText(state, x, y, (char *)enddesc, k, c, &dummyGlyph);
    Graphics_FlushText(state);

    Graphics_SetFontSize(state, currFontSize, FONT_UPSCALE_DEFAULT_SIZE);
    Graphics_PrepareTextRendering(state, &state->projection, &state->scale);

    glDisable(GL_BLEND);
}

int Graphics_RenderView(View *view, OpenGLState *state, Theme *theme, Float dt){
    BufferView *bView = View_GetBufferView(view);
    if(bView->lineBuffer != nullptr){
        //TODO: Maybe generalize this for any linebuffer?
        int is_locked = 0;
        LockedLineBuffer *lockedBuffer = nullptr;
        GetExecutorLockedLineBuffer(&lockedBuffer);
        if(lockedBuffer->lineBuffer == bView->lineBuffer){
            lockedBuffer->mutex.lock();
            is_locked = 1;
        }

        int r =  Graphics_RenderBufferView(view, state, theme, dt);

        if(is_locked){
            if(lockedBuffer->render_state == 1){
                if(r == 0){
                    lockedBuffer->render_state = -1;
                }
            }else if(lockedBuffer->render_state == -1){

            }else{
                r = 1;
            }

            lockedBuffer->mutex.unlock();
        }

        return r;
    }else{
        return Graphics_RenderDefaultView(view, state, theme, dt);
    }
}

int Graphics_RenderBufferView(View *vview, OpenGLState *state, Theme *theme, Float dt){
    Float ones[] = {1,1,1,1};
    char linen[32];
    BufferView *view = View_GetBufferView(vview);

    OpenGLFont *font = &state->font;
    Geometry geometry = view->geometry;

    Float originalScaleWidth = (geometry.upper.x - geometry.lower.x) * font->fontMath.invReduceScale;
    state->renderLineWidth = originalScaleWidth;
    Transform translate;

    vec4f backgroundColor = GetUIColorf(theme, UIBackground);
    vec4f backgroundLineNColor = GetUIColorf(theme, UILineNumberBackground);
    Float fcol[] = { backgroundColor.x, backgroundColor.y, backgroundColor.z,
        backgroundColor.w };
    Float fcolLN[] = { backgroundLineNColor.x, backgroundLineNColor.y,
        backgroundLineNColor.z, backgroundLineNColor.w };

    int is_animating = 0;
    vec2ui cursor = BufferView_GetCursorPosition(view);
    vec2ui visibleLines = BufferView_GetViewRange(view);
    Buffer *cursorBuffer = BufferView_GetBufferAt(view, cursor.x);

    int n = snprintf(linen, sizeof(linen), "%u ", BufferView_GetLineCount(view));
    int pGlyph = -1;
    if(view->renderLineNbs){
        view->lineOffset = fonsComputeStringAdvance(font->fsContext, linen, n, &pGlyph);
    }else{
        view->lineOffset = fonsComputeStringAdvance(font->fsContext, " ",
                                                    1, &pGlyph) * 0.5;
    }

    ActivateViewportAndProjection(state, vview, ViewportAllView);

    glClearBufferfv(GL_COLOR, 0, fcol);
    glClearBufferfv(GL_DEPTH, 0, ones);
    Float baseHeight = 0;
    if(vview->descLocation == DescriptionTop){
        baseHeight = font->fontMath.fontSizeAtRenderCall;
    }

    // Decouple the line numbering for the text viewing, this allows for horizontal
    // scrolling to not affect line numbers.
    if(view->renderLineNbs){
        ActivateViewportAndProjection(state, vview, ViewportLineNumbers);
        glClearBufferfv(GL_COLOR, 0, fcolLN);

        OpenGLRenderAllLineNumbers(state, view, theme);
    }

    geometry.lower.x += view->lineOffset * font->fontMath.reduceScale;
    Float width = geometry.upper.x - geometry.lower.x;
    Float scaledWidth = width * font->fontMath.invReduceScale;

    OpenGLComputeCursor(state, &state->glCursor, cursorBuffer,
                        cursor, 0, baseHeight, visibleLines);

    OpenGLComputeCursorProjection(state, &state->glCursor, &translate, width, view);
    state->model = state->scale * translate;

    if(view->isActive){
        cursor = BufferView_GetGhostCursorPosition(view);
        cursorBuffer = BufferView_GetBufferAt(view, cursor.x);
        OpenGLComputeCursor(state, &state->glGhostCursor, cursorBuffer,
                            cursor, 0, baseHeight, visibleLines);
    }else{
        state->glGhostCursor.valid = 0;
    }

    ActivateViewportAndProjection(state, vview, ViewportText);

    // render alignment and scoped quads first to not need to blend this thing
    Graphics_RenderScopeSections(state, vview, scaledWidth, &state->projection,
                                 &state->model, theme);

    // only render line spacing if there is no git diff going on, otherwise
    // it is might overwhelm our eyes
    if(!Graphics_RenderLineHighlight(state, vview, scaledWidth, &state->projection,
                                     &state->model, theme))
    {
        Graphics_RenderSpacesHighlight(state, vview, &state->projection, theme);
    }

    Graphics_RenderDbgBreaks(state, vview, &state->projection, scaledWidth, theme);

    if(state->bErrors.size() > 0){
        Graphics_RenderBuildErrors(state, vview, scaledWidth, &state->projection,
                                   &state->model, theme);
    }

    if(!BufferView_IsAnimating(view)){
        Graphics_RenderTextBlock(state, view, baseHeight, &state->projection);
        Graphics_RenderCursorElements(state, vview, originalScaleWidth, &state->projection);
    }else{
        vec2ui range;
        vec2ui cursorAt;
        Transform translate;
        Transform model;
        Transition tr = BufferView_GetTransitionType(view);
        int manual = 1;

        switch(tr){
            case TransitionCursor:{
                BufferView_GetCursorTransition(view, dt, &range, &cursorAt, &translate);
            } break;
            case TransitionScroll:{
                BufferView_GetScrollViewTransition(view, dt, &range, &cursorAt, &translate);
            } break;
            case TransitionNumbers:{
                BufferView_GetNumbersShowTransition(view, dt);
                manual = 0;
            } break;

            default:{}
        }
        if(manual){
            model = state->scale * translate;
            cursorBuffer = BufferView_GetBufferAt(view, cursorAt.x);

            OpenGLComputeCursor(state, &state->glCursor, cursorBuffer, cursorAt,
                                0, baseHeight, range);

            _Graphics_RenderTextBlock(state, view, baseHeight, &state->projection,
                                      &model, range);
            _Graphics_RenderCursorElements(state, vview, originalScaleWidth, &model, &state->projection);
        }else{
            Graphics_RenderTextBlock(state, view, baseHeight, &state->projection);
            Graphics_RenderCursorElements(state, vview, originalScaleWidth, &state->projection);
        }
        is_animating = 1;
    }

    ActivateViewportAndProjection(state, vview, ViewportAllView);
    Graphics_RenderFrame(state, vview, &state->projection, originalScaleWidth, theme);

    glDisable(GL_SCISSOR_TEST);
    return is_animating;
}
