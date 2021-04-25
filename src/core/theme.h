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
    UIPasteColor,
}UIElement;

typedef struct{
    // the main background color for the editor
    vec4i backgroundColor;
    // color used to paint active word in hoverable components
    // currently the AutoComplete uses it to specify foreground of active item
    vec4i hoverableItemForegroundColor;
    // color used to paint active word background on hoverable components
    // currently the AutoComplete uses it to specify background of active item
    vec4i hoverableItemBackgroundColor;
    // color used to paint query bar background and default background
    vec4i selectorBackground;
    // color used for the background that is painted for a word when searching
    vec4i searchBackgroundColor;
    // color used for background for list selection view
    vec4i selectableListBackground;
    // color used for the foreground of a word during search
    vec4i searchWordColor;
    // color used to display the background of the line numbers region
    vec4i backgroundLineNumbers;
    // color used to display the line numbers of a file
    vec4i lineNumberColor;
    // color used to display the line number of the line where cursor is located
    vec4i lineNumberHighlitedColor;
    // color of the line where the cursor is at
    vec4i cursorLineHighlight;
    // color for operators, i.e.: if, else, for, ...
    vec4i operatorColor;
    // color for default datatypes, i.e.: int, float,...
    vec4i datatypeColor;
    // color to display comments
    vec4i commentColor;
    // color to display strings, i.e.: "string"
    vec4i stringColor;
    // color to display numbers
    vec4i numberColor;
    // color used for reserved tokens and braces/parenthesis/brackets
    vec4i reservedColor;
    // color used to display functions, i.e.: function()
    vec4i functionColor;
    // color used to display includes, i.e.: #include <value>
    vec4i includeColor;
    // color used for mathematical operators, i.e.: *, +, -, /, ...
    vec4i mathColor;
    // color used for regular words that are not important to the language
    vec4i tokensColor;
    // color used for the character that the cursor is on top of
    vec4i tokensOverCursorColor;
    // color used for preprocessors, i.e.: #define value
    vec4i preprocessorColor;
    // TODO: currently used by enums
    vec4i preprocessorDefineColor;
    // color to use for file borders, if enabled
    vec4i borderColor;
    // color used for braces when rendering nest levels
    vec4i braces;
    // color used for the cursor
    vec4i cursorColor;
    // color used for the query bar cursor
    vec4i querybarCursorColor;
    // color used for the ghost cursor
    vec4i ghostCursorColor;
    // color used for levels of parenthesis
    vec4i parenthesis[4];
    // color used for levels of indentation
    vec4i backTextColors[4];
    // color used for user-defined datatypes from lex process
    vec4i userDatatypeColor;
    // color used for user-defined enum values from lex process
    vec4i userDatatypeEnum;
    // color used to display the line of a block of code
    vec4i scopeLineColor;
    // color used for the filling of scroll bar on the file header
    vec4i scrollbarColor;
    // color used for the string being typed on the query bar
    vec4i querybarTypeLineColor;
    // color used for the 'LOADED' string of the selection file
    vec4i selectorLoadedColor;
    // reserved for testing/development
    vec4i userDefineColor;
    // the amount of levels available for code identation colors
    short backTextCount;
    // the size of the border of the files, if file border is enabled
    Float lineBorderWidth;
    // flag indicating if this theme wants the inactve views to be alpha dimmed
    short alphaDimm;
    // flag indicating if this themes cursor is based on the token position or not
    bool dynamicCursor;
    // the border width when displaying the selector component, i.e.: open files
    // and switch buffer
    int borderWidth;
    // color used to fade in characters when paste is used, alpha value is overwritten
    // during interpolation
    vec4i pasteColor;
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
int GetSelectorBorderWidth(Theme *theme);
#endif //THEME_H
