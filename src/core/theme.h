/* date = January 17th 2021 4:25 pm */

#ifndef THEME_H
#define THEME_H
#include <geometry.h>
#include <lex.h>

typedef struct{
    vec3i backgroundColor;
    vec3i operatorColor;
    vec3i datatypeColor;
    vec3i commentColor;
    vec3i stringColor;
    vec3i numberColor;
    vec3i reservedColor;
    vec3i functionColor;
    vec3i includeColor;
    vec3i mathColor;
    vec3i tokensColor;
    vec3i preprocessorColor;
}Theme;

extern Theme defaultTheme;

vec3i GetColor(Theme *theme, TokenId id);

#endif //THEME_H
