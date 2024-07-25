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

bool render_images(View *view, Geometry *geometry,
                   OpenGLState *state, vec4f backgroundColor)
{
    PdfViewState *pdfView = view->bufferView.pdfView;
    ImageRenderer &renderer = view->bufferView.imgRenderer;
    double factor = renderer.Transition();

    check_for_update(pdfView, renderer);

    glUseProgram(state->imRendererShader.id);
    Shader_UniformVec4(state->imRendererShader, "backgroundColor", backgroundColor);
    Shader_UniformFloat(state->imRendererShader, "transitionFactor", (Float)factor);
    ImageRendererRender(renderer, state->imRendererShader, geometry,
                        PdfView_GetZoomCenter(pdfView), PdfView_GetZoomLevel(pdfView),
                        view->isActive ? 1 : 0);
    return renderer.IsTransitioninig();
}

int Graphics_RenderImage(View *view, OpenGLState *state, Theme *theme, Float dt){
    Geometry geometry;
    BufferView *bView = View_GetBufferView(view);
    BufferView_GetGeometry(bView, &geometry);

    vec4f backgroundColor = GetUIColorf(theme, UISelectorBackground);
    Float fcol[] = { backgroundColor.x, backgroundColor.y, backgroundColor.z,
        backgroundColor.w };

    ActivateViewportAndProjection(state, view, ViewportAllView);
    glClearBufferfv(GL_COLOR, 0, fcol);
    glClearBufferfv(GL_DEPTH, 0, kOnes);

    bool is_transitionining = render_images(view, &geometry, state, backgroundColor);

    return is_transitionining ? 1 : 0;
}
