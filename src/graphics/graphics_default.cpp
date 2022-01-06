#include <graphics.h>
#include <view.h>
#include <glad/glad.h>

/* Renders the default screen (no linebuffers available to the view) */

int Graphics_RenderDefaultView(View *view, OpenGLState *state, Theme *theme, Float dt){
    int pGlyph = -1;
    // TODO: key bindings should be configurable
    const char *line0 = "Press Alt + F to open a file";
    OpenGLFont *font = &state->font;
    uint line0len = strlen(line0);

    Geometry *geometry = &view->geometry;
    vec4f backgroundColor = GetUIColorf(theme, UISelectorBackground);
    vec4i color = GetUIColor(theme, UIScopeLine) * 1.3;
    Float fcol[] = { backgroundColor.x, backgroundColor.y, backgroundColor.z,
        backgroundColor.w };

    uint currFontSize = state->font.fontMath.fontSizeAtDisplay;
    uint fontSize = currFontSize + 16;

    ActivateViewportAndProjection(state, view, ViewportAllView);
    glClearBufferfv(GL_COLOR, 0, fcol);
    glClearBufferfv(GL_DEPTH, 0, kOnes);

    Graphics_SetFontSize(state, fontSize);
    Graphics_PrepareTextRendering(state, &state->projection, &state->scale);
    vec2f p = Graphics_ComputeCenteringStart(font, line0, line0len, geometry);
    pGlyph = -1;

    // move it half a line up
    p.y -= font->fontMath.fontSizeAtRenderCall;

    Graphics_PushText(state, p.x, p.y, (char *)line0, line0len, color, &pGlyph);

    Graphics_FlushText(state);
    Graphics_SetFontSize(state, currFontSize);
    glDisable(GL_SCISSOR_TEST);
    return 0;
}
