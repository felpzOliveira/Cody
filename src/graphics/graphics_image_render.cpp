#include <graphics.h>
#include <view.h>
#include <glad/glad.h>
#include <bufferview.h>
#include <image_renderer.h>

void check_for_update(PdfViewState *pdfView, ImageRenderer &renderer){
    if(PdfView_Changed(pdfView)){
        PdfRenderPages pages = PdfView_GetCurrentPage(pdfView);
        ImageRendererUpdate(renderer, pages);

        PdfView_ClearPendingFlag(pdfView);
    }
}

void update_render_index(PdfViewState *pdfView, int pageIndex, ImageRenderer &renderer){
    PdfGraphicsPage page = PdfView_GetPage(pdfView, pageIndex);
    ImageRendererUpdate(renderer, page);
}

bool should_render_side_view(PdfViewState *pdfView){
    PdfViewGraphicState gView = PdfView_GetGraphicsState(pdfView);
    return PdfView_IsScrolling(pdfView) || IsZero(gView.zoomLevel - 1);
}

void render_side_view(View *view, OpenGLState *state, Geometry *geometry,
                      vec4f textColor, vec4f backgroundColor, Float factor)
{
    char line[32];
    int pGlyph = -1;
    PdfViewState *pdfView = view->bufferView.pdfView;
    vec4i color = ColorFromRGBA(textColor);
    ImageRenderer &scrollRenderer = view->bufferView.scrollImgRenderer;
    OpenGLFont *font = &state->font;
    uint currFontSize = font->fontMath.fontSizeAtDisplay;
    int n = PdfView_EstimateScrollPage(pdfView);
    int len = snprintf(line, sizeof(line), "%d", n);

    uint fontSize = currFontSize + 8;
    Graphics_SetFontSize(state, fontSize);
    Graphics_PrepareTextRendering(state, &state->projection, &state->scale);

    Float dx = fonsComputeStringAdvance(font->fsContext, line,
                                len, &pGlyph, UTF8Encoder()) * 0.5;

    vec2f corner_p0 = vec2f(0.9);
    vec2f corner_p1 = vec2f(1.0);
    Geometry smallGeo = geometry->Region(corner_p0, corner_p1);

    Geometry rectGeo = smallGeo;
    rectGeo.lower.y -= 0.04 * geometry->Height();

    Float y = geometry->Height() * font->fontMath.invReduceScale *
                            (1.0f - corner_p0.y);
    Float x = geometry->Width() * font->fontMath.invReduceScale *
                            (corner_p0.x + corner_p1.x) * 0.5f - dx;

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

    if(should_render_side_view(pdfView)){
        render_side_view(view, state, geometry, textColor, backgroundColor, factor);
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
