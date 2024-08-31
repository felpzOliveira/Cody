#include <graphics.h>
#include <view.h>
#include <glad/glad.h>
#include <bufferview.h>
#include <image_renderer.h>

void check_for_update(PdfViewState *pdfView, ImageRenderer &renderer){
    if(PdfView_Changed(pdfView)){
        int width, height;
        const char *ptr = PdfView_GetCurrentPage(pdfView, width, height);
        ImageRendererUpdate(renderer, width, height, (unsigned char *)ptr);

        PdfView_ClearPendingFlag(pdfView);
    }
}

void update_render_index(PdfViewState *pdfView, int pageIndex, ImageRenderer &renderer){
    int width, height;
    const char *ptr = PdfView_GetImagePage(pdfView, pageIndex, width, height);
    ImageRendererUpdate(renderer, width, height, (unsigned char *)ptr);
}

bool render_side_view(PdfViewState *pdfView){
    PdfViewGraphicState gView = PdfView_GetGraphicsState(pdfView);
    return PdfView_IsScrolling(pdfView) || IsZero(gView.zoomLevel - 1);
}

bool render_images(View *view, Geometry *geometry, OpenGLState *state,
                   vec4f textColor, vec4f backgroundColor)
{
    PdfViewState *pdfView = view->bufferView.pdfView;
    ImageRenderer &renderer = view->bufferView.imgRenderer;
    double factor = renderer.Transition();

    check_for_update(pdfView, renderer);

    ImageRendererRender(renderer, state->imRendererShader, state->imBorderShader,
                        backgroundColor, (Float)factor, geometry,
                        PdfView_GetZoomCenter(pdfView), PdfView_GetZoomLevel(pdfView),
                        view->isActive ? 1 : 0);

    if(render_side_view(pdfView)){
        char line[32];
        int pGlyph = -1;
        vec4i color = ColorFromRGBA(textColor);
        ImageRenderer &scrollRenderer = view->bufferView.scrollImgRenderer;
        OpenGLFont *font = &state->font;
        uint currFontSize = font->fontMath.fontSizeAtDisplay;
        int n = PdfView_EstimateScrollPage(pdfView);
        int len = snprintf(line, sizeof(line), "%d", n);

        uint fontSize = currFontSize + 8;
        Graphics_SetFontSize(state, fontSize);
        Graphics_PrepareTextRendering(state, &state->projection, &state->scale);

        Float w = geometry->Width();
        Float h = geometry->Height();

        Float dx = fonsComputeStringAdvance(font->fsContext, line,
                                        len, &pGlyph, UTF8Encoder()) * 0.5;

        Float _x = 0.9f * (Float)geometry->upper.x;
        Float _y = 0.9f * (Float)geometry->upper.y;
        Float x = _x * font->fontMath.invReduceScale * 1.05f;
        Float y = (h - _y) * font->fontMath.invReduceScale;

        x = x - dx;

        Geometry smallGeo;
        smallGeo.lower = vec2ui(static_cast<uint>(_x), static_cast<uint>(_y));
        smallGeo.upper = vec2ui(static_cast<uint>(_x + 0.1 * w),
                                static_cast<uint>(_y + 0.1 * h));

        Geometry rectGeo = smallGeo;
        rectGeo.lower.y -= 0.04 * h;
        update_render_index(pdfView, n, scrollRenderer);

        ActivateViewportAndProjection(state, &rectGeo, true);
        Float fcol[] = { backgroundColor.x, backgroundColor.y,
                         backgroundColor.z, backgroundColor.w };
        glClearBufferfv(GL_COLOR, 0, fcol);

        ActivateViewportAndProjection(state, view, ViewportAllView);
        Graphics_PushText(state, x, y, (char *)line, len, color, &pGlyph, UTF8Encoder());
        Graphics_FlushText(state);
        Graphics_SetFontSize(state, currFontSize);
        Graphics_PrepareTextRendering(state, &state->projection, &state->scale);

        ActivateViewportAndProjection(state, &smallGeo, true);
        ImageRendererRender(scrollRenderer, state->imRendererShader, state->imBorderShader,
                        backgroundColor, (Float)factor, &smallGeo,
                        vec2f(0.5,0.5), 1.f, 0);

    }

    return renderer.IsTransitioninig();
}

int Graphics_RenderImage(View *view, OpenGLState *state, Theme *theme, Float dt){
    Geometry geometry;
    BufferView *bView = View_GetBufferView(view);
    BufferView_GetGeometry(bView, &geometry);

    vec4f backgroundColor = GetUIColorf(theme, UISelectorBackground);
    vec4f textColor = GetColorf(theme, TOKEN_ID_NONE);
    Float fcol[] = { backgroundColor.x, backgroundColor.y, backgroundColor.z,
                     backgroundColor.w };

    ActivateViewportAndProjection(state, view, ViewportAllView);
    glClearBufferfv(GL_COLOR, 0, fcol);
    glClearBufferfv(GL_DEPTH, 0, kOnes);

    bool is_transitionining = render_images(view, &geometry, state,
                                            textColor, backgroundColor);

    return is_transitionining ? 1 : 0;
}
