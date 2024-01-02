#include <graphics.h>
#include <view.h>
#include <view_tree.h>
#include <control_cmds.h>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <app.h>

// TODO(29/11/22): Review or re-do the tmux-like rendering of views ids.
//                 We are again viewing errors in this routine as no id is shown
//                 but the event is actually happening, i.e.: not able to do anything
//                 as it is in the ctrl+b lock but no id is shown for ctrl+b+q
//                 this could be a timer thing, a rendering thing or an event thing
//                 since we cannot be sure as this is hard to duplicate in debug
//                 re-write this.
int Graphics_RenderControlCmdsIndices(View *view, OpenGLState *state,
                                      Theme *theme, Float dt, ControlProps *props)
{
    char line[4];
    ViewTreeIterator iterator;
    uint len = 0;
    int id = -1;
    OpenGLFont *font = &state->font;
    uint currFontSize = font->fontMath.fontSizeAtDisplay;
    uint fontSize = currFontSize + 140; // TODO: Can we compute this size?
    int pGlyph = -1;
    Geometry *geometry = &view->geometry;
    vec4i color(255, 0, 0, 255);
    if(view->isActive){
        color = vec4i(0, 100, 255, 255);
    }

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
    Graphics_FlushText(state);

    Graphics_SetFontSize(state, fontSize, fontSize + FONT_UPSCALE_DEFAULT_OFFSET);
    Graphics_PrepareTextRendering(state, &state->projection, &state->scale);

    Float y = (geometry->upper.y - geometry->lower.y) * font->fontMath.invReduceScale * 0.5;
    Float x = (geometry->upper.x - geometry->lower.x) * font->fontMath.invReduceScale * 0.5;

    // NOTE: This renders the numbers tmux-like we don't really care about encoders here
    Float dx = fonsComputeStringAdvance(font->fsContext, line,
                                        len, &pGlyph, UTF8Encoder()) * 0.5;
    x = x - dx;
    y -= font->fontMath.fontSizeAtRenderCall;

    Graphics_PushText(state, x, y, (char *)line, len, color, &pGlyph, UTF8Encoder());
    Graphics_FlushText(state);
    Graphics_SetFontSize(state, currFontSize, FONT_UPSCALE_DEFAULT_SIZE);
    Graphics_PrepareTextRendering(state, &state->projection, &state->scale);

    return 1;
}

int Graphics_RenderControlCommands(View *view, OpenGLState *state,
                                   Theme *theme, Float dt)
{
    ControlProps *props = nullptr;
    View_GetControlRenderOpts(view, &props);
    int animating = 0;

    if(AppGetRenderViewIndices())
        animating = Graphics_RenderControlCmdsIndices(view, state, theme, dt, props);

    return animating;
}
