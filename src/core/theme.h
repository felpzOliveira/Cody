/* date = January 17th 2021 4:25 pm */

#ifndef THEME_H
#define THEME_H
#include <geometry.h>
#include <lex.h>

typedef enum{
    UIBorder,
}UIElement;

typedef struct{
    vec4i backgroundColor;
    vec4i operatorColor;
    vec4i datatypeColor;
    vec4i commentColor;
    vec4i stringColor;
    vec4i numberColor;
    vec4i reservedColor;
    vec4i functionColor;
    vec4i includeColor;
    vec4i mathColor;
    vec4i tokensColor;
    vec4i preprocessorColor;
    vec4i preprocessorDefineColor;
    vec4i borderColor;
    
    vec4i testColor;
}Theme;

extern Theme defaultTheme;

void SetAlpha(int acitve);
vec4i GetColor(Theme *theme, TokenId id);
vec4f GetColorf(Theme *theme, TokenId id);
vec4i GetUIColor(Theme *theme, UIElement id);
vec4f GetUIColorf(Theme *theme, UIElement id);

#endif //THEME_H
