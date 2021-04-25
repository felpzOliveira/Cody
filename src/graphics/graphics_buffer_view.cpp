#include <graphics.h>
#include <glad/glad.h>
#include <gl3corefontstash.h>
#include <vector>
#include <app.h>
#include <file_provider.h>

void _Graphics_RenderCursorElements(OpenGLState *state, BufferView *bufferView, 
                                    Transform *model, Transform *projection);
void _Graphics_RenderCursorElements(OpenGLState *state, BufferView *bufferView, 
                                    Transform *model, Transform *projection);
int OpenGLRenderLine(BufferView *view, OpenGLState *state,
                     Float &x, Float &y, uint lineNr);


static Float ComputeTransformOf(Float x, Transform *transform){
    vec3f p(x, 0, 0);
    p = transform->Point(p);
    return p.x;
}

void OpenGLRenderEmptyCursorRect(OpenGLState *state, vec2f lower, vec2f upper, vec4f color, uint thickness)
{
    Float x0 = lower.x, y0 = lower.y, x1 = upper.x, y1 = upper.y;
    OpenGLBuffer *quad = &state->glQuadBuffer;
    if(quad->length + 24 > quad->size){
        Graphics_QuadFlush(state);
    }
    uint w = thickness;
    Graphics_QuadPush(state, vec2ui(x0+0, y0+0), vec2ui(x0+w, y1+0), color);
    Graphics_QuadPush(state, vec2ui(x0+0, y1+0), vec2ui(x1+w, y1+w), color);
    Graphics_QuadPush(state, vec2ui(x1+0, y1+0), vec2ui(x1+w, y0+w), color);
    Graphics_QuadPush(state, vec2ui(x1+w, y0+w), vec2ui(x0+0, y0+0), color);
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

void OpenGLComputeCursor(OpenGLState *state, OpenGLCursor *glCursor, 
                         Buffer *buffer, vec2ui cursor, Float cursorOffset, 
                         Float baseHeight, vec2ui visibleLines)
{
    Float x0 = 0, x1 = 0, y0 = 0, y1 = 0;
    uint rawp = 0;
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
        
        rawp = Buffer_Utf8PositionToRawPosition(buffer, cursor.y, &pUtf8Len);
        x1 = x0 + fonsComputeStringAdvance(state->font.fsContext, &ptr[rawp],
                                           pUtf8Len, &glCursor->pGlyph);
        
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

void OpenGLRenderCursor(OpenGLState *state, OpenGLCursor *glCursor, vec4f color,
                        uint thickness, int isActive)
{
    Float x0 = glCursor->pMin.x, x1 = glCursor->pMax.x;
    Float y0 = glCursor->pMin.y, y1 = glCursor->pMax.y;
    if(isActive){
        if(appGlobalConfig.cStyle == CURSOR_RECT){
            Graphics_QuadPush(state, vec2ui(x0, y0), vec2ui(x1, y1), color);
        }else{
            x0 = x0 > 3 ? x0 - 3 : 0;
            Graphics_QuadPush(state, vec2ui(x0, y0), vec2ui(x0+3, y1), color);
        }
    }else{
        if(appGlobalConfig.cStyle == CURSOR_RECT){
            OpenGLRenderEmptyCursorRect(state, vec2f(x0, y0), vec2f(x1, y1), 
                                        color, thickness);
        }else{
            x0 = x0 > 3 ? x0 - 3 : 0;
            OpenGLRenderEmptyCursorRect(state, vec2f(x0, y0), vec2f(x0+3, y1), 
                                        color, thickness);
        }
    }
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
    vec4i s(128);
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

void _Graphics_RenderCursorElements(OpenGLState *state, View *view, 
                                    Transform *model, Transform *projection)
{
    int pGlyph = -1;
    BufferView *bufferView = View_GetBufferView(view);
    ViewState vstate = View_GetState(view);
    OpenGLFont *font = &state->font;
    OpenGLCursor *cursor = &state->glCursor;
    Buffer *buffer = BufferView_GetBufferAt(bufferView, cursor->textPos.x);
    
    if(vstate == View_FreeTyping){
        vec4f col = Graphics_GetCursorColor(bufferView, defaultTheme);
        glUseProgram(font->cursorShader.id);
        Shader_UniformMatrix4(font->cursorShader, "projection", &projection->m);
        Shader_UniformMatrix4(font->cursorShader, "modelView", &model->m);
        
        glDisable(GL_BLEND);
        OpenGLRenderCursor(state, &state->glCursor, col,
                           4, bufferView->isActive && vstate == View_FreeTyping);
        
        if(state->glGhostCursor.valid){
            col = Graphics_GetCursorColor(bufferView, defaultTheme, 1);
            OpenGLRenderCursor(state, &state->glGhostCursor, col, 2, 0);
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

void Graphics_RenderCursorElements(OpenGLState *state, View *view, Transform *projection){
    _Graphics_RenderCursorElements(state, view, &state->model, projection);
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
            Float minY = -font->fontMath.fontSizeAtRenderCall;;
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
        
        Graphics_QuadFlush(state, 0);
        
        Shader_UniformMatrix4(font->cursorShader, "modelView", &model->m);
        Graphics_LineFlush(state);
        
    }
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
    Float pc = BufferView_GetDescription(view, desc, sizeof(desc));
    
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
    
    fonsStashFlush(font->fsContext);
    glDisable(GL_BLEND);
}

int Graphics_RenderView(View *view, OpenGLState *state, Theme *theme, Float dt){
    BufferView *bView = View_GetBufferView(view);
    if(bView->lineBuffer != nullptr){
        return Graphics_RenderBufferView(view, state, theme, dt);
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
    
    if(!BufferView_IsAnimating(view)){
        Graphics_RenderTextBlock(state, view, baseHeight, &state->projection);
        Graphics_RenderCursorElements(state, vview, &state->projection);
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
            _Graphics_RenderCursorElements(state, vview, &model, &state->projection);
        }else{
            Graphics_RenderTextBlock(state, view, baseHeight, &state->projection);
            Graphics_RenderCursorElements(state, vview, &state->projection);
        }
        is_animating = 1;
    }
    
    ActivateViewportAndProjection(state, vview, ViewportAllView);
    Graphics_RenderFrame(state, vview, &state->projection, originalScaleWidth, theme);

    glDisable(GL_SCISSOR_TEST);
    return is_animating;
}
