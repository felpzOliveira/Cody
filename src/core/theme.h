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
    UIHoverableListItem,
    UIHoverableListItemBackground,
}UIElement;

typedef struct{
    vec4i backgroundColor; // the main background color for the editor
    vec4i hoverableItemForegroundColor; // color used to paint active word in hoverable components
    vec4i hoverableItemBackgroundColor; // color used to paint active word background on hoverable components
    vec4i selectorBackground; // color used to paint query bar background and default background
    vec4i searchBackgroundColor; // color used for the background that is painted for a word when searching
    vec4i selectableListBackground; // color used for background for list selection view
    vec4i searchWordColor; // color used for the foreground of a word during search
    vec4i backgroundLineNumbers; // color used to display the background of the line numbers region
    vec4i lineNumberColor; // color used to display the line numbers of a file
    vec4i lineNumberHighlitedColor; // color used to display the line number of the line where cursor is located
    vec4i cursorLineHighlight; // color of the line where the cursor is at
    vec4i operatorColor; // color for operators, i.e.: if, else, for, ...
    vec4i datatypeColor; // color for default datatypes, i.e.: int, float,...
    vec4i commentColor; // color to display comments
    vec4i stringColor; // color to display strings, i.e.: "string"
    vec4i numberColor; // color to display numbers
    vec4i reservedColor; // color used for reserved tokens and braces/parenthesis/brackets
    vec4i functionColor; // color used to display functions, i.e.: function()
    vec4i includeColor; // color used to display includes, i.e.: #include <value>
    vec4i mathColor; // color used for mathematical operators, i.e.: *, +, -, /, ...
    vec4i tokensColor; // color used for regular words that are not important to the language
    vec4i tokensOverCursorColor; // color used for the character that the cursor is on top of
    vec4i preprocessorColor; // color used for preprocessors, i.e.: #define value
    vec4i preprocessorDefineColor; // TODO: currently used by enums
    vec4i borderColor; // color to use for file borders, if enabled
    vec4i braces; // color used for braces when rendering nest levels
    vec4i cursorColor; // color used for the cursor
    vec4i querybarCursorColor; // color used for the query bar cursor
    vec4i ghostCursorColor; // color used for the ghost cursor
    vec4i parenthesis[4]; // color used for levels of parenthesis
    vec4i backTextColors[4]; // color used for levels of indentation
    vec4i userDatatypeColor; // color used for user-defined datatypes from lex process
    vec4i userDatatypeEnum; // color used for user-defined enum values from lex process
    vec4i scopeLineColor; // color used to display the line of a block of code
    vec4i scrollbarColor; // color used for the filling of scroll bar on the file header
    vec4i querybarTypeLineColor; // color used for the string being typed on the query bar
    vec4i selectorLoadedColor; // color used for the 'LOADED' string of the selection file
    vec4i userDefineColor; // reserved for testing/development
    short backTextCount; // the amount of levels available for code identation colors
    Float lineBorderWidth; // the size of the border of the files, if file border is enabled
    short alphaDimm; // flag indicating if this theme wants the inactve views to be alpha dimmed
    bool dynamicCursor; // flag indicating if this themes cursor is based on the token position or not
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
