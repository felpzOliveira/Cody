#include <graphics.h>
#include <view.h>
#include <view_tree.h>
#include <control_cmds.h>

int Graphics_RenderControlCmdsIndices(View *view, OpenGLState *state,
                                      Theme *theme, Float dt, ControlProps *props)
{
    char line[4];
    ViewTreeIterator iterator;
    int animating = 0;
    uint len = 0;
    Float remaining = 0;
    int id = -1;
    OpenGLFont *font = &state->font;
    uint currFontSize = font->fontMath.fontSizeAtDisplay;
    uint fontSize = currFontSize + 104; // TODO: Can we compute this size?
    int pGlyph = -1;
    Geometry *geometry = &view->geometry;
    vec4i color(255, 0, 0, 255);
    if(view->isActive){
        color = vec4i(0, 100, 255, 255);
    }

    props->executedPassed += dt;
    remaining = props->executedInterval - props->executedPassed;

    animating = remaining > 0 ? 1 : 0;

    // Compute the view tree id, can we please make this faster, I don't like to this
    // tree query at render time
    ViewTree_Begin(&iterator);
    if(iterator.value) id = 1;

    while(iterator.value){
        if(iterator.value->view == view){
            break;
        }

        id++;
        ViewTree_Next(&iterator);
    }

    AssertA(id >= 1, "Invalid view id");

    len = snprintf(line, sizeof(line), "%d", id);

    // Render the number at center
    ActivateViewportAndProjection(state, view, ViewportAllView);
    Graphics_SetFontSize(state, fontSize);
    Graphics_PrepareTextRendering(state, &state->projection, &state->scale);

    Float y = (geometry->upper.y - geometry->lower.y) * font->fontMath.invReduceScale * 0.5;
    Float x = (geometry->upper.x - geometry->lower.x) * font->fontMath.invReduceScale * 0.5;
    Float dx = fonsComputeStringAdvance(font->fsContext, line, len, &pGlyph) * 0.5;
    x = x - dx;
    y -= font->fontMath.fontSizeAtRenderCall;

    Graphics_PushText(state, x, y, (char *)line, len, color, &pGlyph);
    Graphics_FlushText(state);
    Graphics_SetFontSize(state, currFontSize);

    // TODO: Can we do this somewhere else?
    if(animating == 0){
        // we need to return 1 here because we need 1 more cycle of
        // rendering to clear what we just rendered
        View_SetControlOpts(view, Control_Opts_None);
        animating = 1;

        // TODO: This should be on a timer callback, however we don't support
        // that yet and theres the hole treading issue
        ControlCommands_YieldKeyboard();
    }
    return animating;
}

int Graphics_RenderControlCommands(View *view, OpenGLState *state,
                                   Theme *theme, Float dt)
{
    ControlProps *props = nullptr;
    View_GetControlRenderOpts(view, &props);
    int animating = 0;

    switch(props->opts){
        case Control_Opts_Indices:{
            animating = Graphics_RenderControlCmdsIndices(view, state, theme, dt, props);
        } break;

        default:{
            animating = 0;
        }
    }

    return animating;
}
