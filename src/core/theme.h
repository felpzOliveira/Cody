/* date = January 17th 2021 4:25 pm */

#ifndef THEME_H
#define THEME_H
#include <geometry.h>
#include <lex.h>
#include <vector>

typedef enum{
    UIBackground,
    UISelectorBackground,
    UISearchBackground,
    UISearchWord,
    UILineNumberBackground,
    UILineNumbers,
    UILineNumberHighlighted,
    UICursorLineHighlight,
    UIBorder,
    UICursor,
    UICharOverCursor,
    UIQueryBarCursor,
    UIQueryBarTypeColor,
    UISelectableListBackground,
    UIGhostCursor,
    UIScopeLine,
    UIScrollBarColor,
    UISelectorLoadedColor,
}UIElement;

typedef struct{
    vec4i backgroundColor;
    vec4i selectorBackground;
    vec4i searchBackgroundColor;
    vec4i selectableListBackground;
    vec4i searchWordColor;
    vec4i backgroundLineNumbers;
    vec4i lineNumberColor;
    vec4i lineNumberHighlitedColor;
    vec4i cursorLineHighlight;
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
    vec4i tokensOverCursorColor;
    vec4i preprocessorColor;
    vec4i preprocessorDefineColor;
    vec4i borderColor;
    vec4i braces;
    vec4i cursorColor;
    vec4i querybarCursorColor;
    vec4i ghostCursorColor;
    vec4i parenthesis[4];
    vec4i backTextColors[4];
    vec4i userDatatypeColor;
    vec4i userDatatypeEnum;
    vec4i scopeLineColor;
    vec4i scrollbarColor;
    vec4i querybarTypeLineColor;
    vec4i selectorLoadedColor;
    vec4i testColor;
    short backTextCount;
    Float lineBorderWidth;
    short alphaDimm;
}Theme;

typedef struct{
    const char *name;
    Theme *theme;
}ThemeDescription;

extern Theme *defaultTheme;

void SetAlpha(int acitve);
vec4i GetNestColor(Theme *theme, TokenId id, int level);
vec4f GetNestColorf(Theme *theme, TokenId id, int level);
vec4i GetColor(Theme *theme, TokenId id);
vec4f GetColorf(Theme *theme, TokenId id);
vec4i GetUIColor(Theme *theme, UIElement id);
vec4f GetUIColorf(Theme *theme, UIElement id);
Float GetLineBorderWidth(Theme *theme);
short GetBackTextCount(Theme *theme);
void GetThemeList(std::vector<ThemeDescription> **themes);
void SwapDefaultTheme(char *name, uint len);
int ThemeNeedsEffect(Theme *theme);
#endif //THEME_H
