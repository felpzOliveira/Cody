#include <theme.h>
#include <utilities.h>

Theme themeDracula = {
    .backgroundColor = ColorFromHex(0xFF282a36),
    .selectorBackground = ColorFromHex(0xFF363a47),
    .searchBackgroundColor = ColorFromHex(0xFF44475a),
    .selectableListBackground = ColorFromHex(0xFF363a47),
    .searchWordColor = ColorFromHex(0xCC8e8620),
    .backgroundLineNumbers = ColorFromHex(0xFF282a36),
    .lineNumberColor = ColorFromHex(0xFF786fa8),
    .lineNumberHighlitedColor = ColorFromHex(0xFF786fa8),
    .cursorLineHighlight = ColorFromHex(0xFF44475a),
    .operatorColor   = ColorFromHex(0xCCD38545),
    .datatypeColor   = ColorFromHex(0xCCff79c6),
    .commentColor    = ColorFromHex(0xFF6272a4),
    .stringColor     = ColorFromHex(0xFFf1fa8c),
    .numberColor     = ColorFromHex(0xFF6B8E23),
    .reservedColor   = ColorFromHex(0xFF6B8E23),
    .functionColor   = ColorFromHex(0xFF50fa7b),
    .includeColor    = ColorFromHex(0xFFf1fa8c),
    .mathColor       = ColorFromHex(0xFFD2D2D3),
    .tokensColor     = ColorFromHex(0xFFf8f8f2),
    .preprocessorColor = ColorFromHex(0xFFDAB98F),
    .preprocessorDefineColor = ColorFromHex(0xAADAB98F),
    .borderColor = ColorFromHex(0xFFFF7F50),
    .braces = ColorFromHex(0xFF00FFFF),
    .cursorColor = ColorFromHex(0xFF40FF40),
    .querybarCursorColor = ColorFromHex(0xFFFF4040),
    .ghostCursorColor = ColorFromHex(0xFF5B4D3C),
    .parenthesis = {
        ColorFromHex(0xFFFF0000),
        ColorFromHex(0xFF00FF00),
        ColorFromHex(0xFF0000FF),
        ColorFromHex(0xFFAAAA00),
    },
    .backTextColors = {
        ColorFromHex(0xFF282a36),
    },
    .userDatatypeColor = ColorFromHex(0xCC8EE7BF),
    .userDatatypeEnum = ColorFromHex(0xCCd8ddad),
    .scopeLineColor = ColorFromHex(0xAAAAAAAA),
    .testColor = ColorFromHex(0xCCff5555),
    .backTextCount = 1,
    .lineBorderWidth = 0,
    .alphaDimm = 0,
};

Theme themeYavid = {
    .backgroundColor = ColorFromHex(0xFF16191C),
    .selectorBackground = ColorFromHex(0xFF17171D),
    .searchBackgroundColor = ColorFromHex(0xFF315268),
    .selectableListBackground = ColorFromHex(0xFF363a47),
    .searchWordColor = ColorFromHex(0xCC8e8620),
    .backgroundLineNumbers = ColorFromHex(0xFF192023),
    .lineNumberColor = ColorFromHex(0xFF4C4D4E),
    .lineNumberHighlitedColor = ColorFromHex(0xFFF9F901),
    .cursorLineHighlight = ColorFromHex(0xFF272729),
    .operatorColor   = ColorFromHex(0xCCD38545),
    .datatypeColor   = ColorFromHex(0xCC719FAE),
    .commentColor    = ColorFromHex(0xFF7D7D7D),
    .stringColor     = ColorFromHex(0xFF6B8E23),
    .numberColor     = ColorFromHex(0xFF6B8E23),
    .reservedColor   = ColorFromHex(0xFF6B8E23),
    .functionColor   = ColorFromHex(0xFFDFDA77),
    .includeColor    = ColorFromHex(0xFF6B8E23),
    .mathColor       = ColorFromHex(0xFFD2D2D3),
    .tokensColor     = ColorFromHex(0xFFD2D2D3),
    .preprocessorColor = ColorFromHex(0xFFDAB98F),
    .preprocessorDefineColor = ColorFromHex(0xAADAB98F),
    .borderColor = ColorFromHex(0xFFFF7F50),
    .braces = ColorFromHex(0xFF00FFFF),
    .cursorColor = ColorFromHex(0xFF40FF40),
    .querybarCursorColor = ColorFromHex(0xFFFF4040),
    .ghostCursorColor = ColorFromHex(0xFF5B4D3C),
    .parenthesis = {
        ColorFromHex(0xFFFF0000),
        ColorFromHex(0xFF00FF00),
        ColorFromHex(0xFF0000FF),
        ColorFromHex(0xFFAAAA00),
    },
    .backTextColors = {
        ColorFromHex(0xFF202020),
    },
    .userDatatypeColor = ColorFromHex(0xCC8EE7BF),
    .userDatatypeEnum = ColorFromHex(0xCCd8ddad),
    .scopeLineColor = ColorFromHex(0xAAAAAAAA),
    .testColor = ColorFromHex(0xCCCD950C),
    .backTextCount = 1,
    .lineBorderWidth = 0,
    .alphaDimm = 0,
};

Theme themeRadical = {
    .backgroundColor = ColorFromHex(0xFF141323),
    .selectorBackground = ColorFromHex(0xFF131220),
    .searchBackgroundColor = ColorFromHex(0xFF342640),
    
    .selectableListBackground = ColorFromHex(0x082f317c),
    
    .searchWordColor = ColorFromHex(0xCCce0b95),
    .backgroundLineNumbers = ColorFromHex(0xFF141323),
    .lineNumberColor = ColorFromHex(0xFF767585),
    .lineNumberHighlitedColor = ColorFromHex(0xFF767585),
    .cursorLineHighlight = ColorFromHex(0xFF241630),
    .operatorColor   = ColorFromHex(0xCCc7ff00),
    
    .datatypeColor   = ColorFromHex(0xCCff427c),
    .commentColor    = ColorFromHex(0x48cff0e9),
    .stringColor     = ColorFromHex(0xCCbaf7fc),
    .numberColor     = ColorFromHex(0xCCfffc7f),
    .reservedColor   = ColorFromHex(0xCCf862b9),
    .functionColor   = ColorFromHex(0xFF54757c),
    .includeColor    = ColorFromHex(0xCCbaf7fc),
    .mathColor       = ColorFromHex(0xCCff5395),
    .tokensColor     = ColorFromHex(0xCCcbfff2),
    .preprocessorColor = ColorFromHex(0xCCff4271),
    .preprocessorDefineColor = ColorFromHex(0xAADAB98F),
    //.preprocessorDefineColor = ColorFromHex(0xFFDD1A01),
    .borderColor = ColorFromHex(0xFFFF7F50),
    .braces = ColorFromHex(0xFF00FFFF),
    .cursorColor = ColorFromHex(0xFFfe428e),
    .querybarCursorColor = ColorFromHex(0xFFFF4040),
    .ghostCursorColor = ColorFromHex(0xFFfe428e),
    .parenthesis = {
        ColorFromHex(0xFFFF0000),
        ColorFromHex(0xFF00FF00),
        ColorFromHex(0xFF0000FF),
        ColorFromHex(0xFFAAAA00),
    },
    .backTextColors = {
        ColorFromHex(0xFF190A25),
    },
    
    .userDatatypeColor = ColorFromHex(0xCCff427c),
    .userDatatypeEnum = ColorFromHex(0xCCd8ddad),
    .scopeLineColor = ColorFromHex(0xAAd5348f),
    .testColor = ColorFromHex(0xCCfa62b9),
    .backTextCount = 1,
    .lineBorderWidth = 0,
    .alphaDimm = 1,
};


Theme theme4Coder = {
    .backgroundColor = ColorFromHex(0xFF0C0C0C),
    .selectorBackground = ColorFromHex(0xFF17171D),
    .searchBackgroundColor = ColorFromHex(0xFF315268),
    .selectableListBackground = ColorFromHex(0xFF1C1C1C),
    .searchWordColor = ColorFromHex(0xCC8e8620),
    .backgroundLineNumbers = ColorFromHex(0xFF171717),
    .lineNumberColor = ColorFromHex(0xFF4C4D4E),
    .lineNumberHighlitedColor = ColorFromHex(0xFFF9F901),
    .cursorLineHighlight = ColorFromHex(0xFF232330),
    .operatorColor   = ColorFromHex(0xCCCD950C),
    
    .datatypeColor   = ColorFromHex(0xCCCD950C),
    .commentColor    = ColorFromHex(0xCC7D7D7D),
    .stringColor     = ColorFromHex(0xCC6B8E23),
    .numberColor     = ColorFromHex(0xCC6B8E23),
    .reservedColor   = ColorFromHex(0xCCA08563),
    .functionColor   = ColorFromHex(0xCC6B8E23),
    .includeColor    = ColorFromHex(0xCC6B8E23),
    .mathColor       = ColorFromHex(0xCCAB8E23),
    .tokensColor     = ColorFromHex(0xDDA08563),
    .preprocessorColor = ColorFromHex(0xCCDAB98F),
    .preprocessorDefineColor = ColorFromHex(0xAADAB98F),
    //.preprocessorDefineColor = ColorFromHex(0xFFDD1A01),
    .borderColor = ColorFromHex(0xFFFF7F50),
    .braces = ColorFromHex(0xFF00FFFF),
    .cursorColor = ColorFromHex(0xFF40FF40),
    .querybarCursorColor = ColorFromHex(0xFFFF4040),
    .ghostCursorColor = ColorFromHex(0xFF5B4D3C),
    .parenthesis = {
        ColorFromHex(0xFFFF0000),
        ColorFromHex(0xFF00FF00),
        ColorFromHex(0xFF0000FF),
        ColorFromHex(0xFFAAAA00),
    },
    .backTextColors = {
        //ColorFromHex(0xFFAAAAAA),
        ColorFromHex(0xFF130707),
        ColorFromHex(0xFF071307),
        ColorFromHex(0xFF070713),
        ColorFromHex(0xFF131307),
    },
    
    //.userDatatypeColor = ColorFromHex(0xCC8EE7BF),
    .userDatatypeColor = ColorFromHex(0xCCEFAC0C),
    .userDatatypeEnum = ColorFromHex(0xCCd8ddad),
    //.userDatatypeColor = ColorFromHex(0xCC00CCCC),
    //.testColor = ColorFromHex(0xCCE91E63),
    .scopeLineColor = ColorFromHex(0xAAAAAAAA),
    .testColor = ColorFromHex(0xCCd04c6a),
    .backTextCount = 4,
    .lineBorderWidth = 0,
    .alphaDimm = 1,
};

//Theme *defaultTheme = &themeRadical;
Theme *defaultTheme = &theme4Coder;
//Theme *defaultTheme = &themeDracula;

static int globalActive = 0;
void SetAlpha(int active){
    globalActive = active;
}

template<typename T>
inline static T ApplyAlpha(T color, Theme *theme){
    if(theme->alphaDimm){
        T s = color;
        if(globalActive) s.w *= kAlphaReduceInactive;
        else s.w *= kAlphaReduceDefault;
        return s;
    }
    
    return color;
}

vec4i GetNestColor(Theme *theme, TokenId id, int level){
    if(id == TOKEN_ID_BRACE_OPEN || id == TOKEN_ID_BRACE_CLOSE)
    {
        return ApplyAlpha(theme->braces, theme);
    }else if(id == TOKEN_ID_PARENTHESE_OPEN || id == TOKEN_ID_PARENTHESE_CLOSE){
        level = level % 4;
        return ApplyAlpha(theme->parenthesis[level], theme);
    }else if(id == TOKEN_ID_SCOPE){
        if(theme->backTextCount > 1){
            level = Clamp(level, 0, theme->backTextCount-1);
        }else{
            level = 0;
        }
        
        return ApplyAlpha(theme->backTextColors[level], theme);
    }
    
    AssertErr(0, "Invalid theme query for nesting color");
    return vec4i(255, 255, 0, 255);
}

vec4i GetColor(Theme *theme, TokenId id){
    vec4i c;
#define COLOR_RET(i, v) case i : c = theme->v; break;
    switch(id){
        COLOR_RET(TOKEN_ID_OPERATOR, operatorColor);
        COLOR_RET(TOKEN_ID_DATATYPE, datatypeColor);
        COLOR_RET(TOKEN_ID_COMMENT, commentColor);
        COLOR_RET(TOKEN_ID_STRING, stringColor);
        COLOR_RET(TOKEN_ID_NUMBER, numberColor);
        COLOR_RET(TOKEN_ID_RESERVED, reservedColor);
        COLOR_RET(TOKEN_ID_FUNCTION, functionColor);
        COLOR_RET(TOKEN_ID_FUNCTION_DECLARATION, functionColor);
        COLOR_RET(TOKEN_ID_INCLUDE, includeColor);
        COLOR_RET(TOKEN_ID_MATH, mathColor);
        COLOR_RET(TOKEN_ID_ASTERISK, mathColor);
        COLOR_RET(TOKEN_ID_NONE, tokensColor);
        COLOR_RET(TOKEN_ID_PREPROCESSOR, preprocessorColor);
        COLOR_RET(TOKEN_ID_PREPROCESSOR_DEFINE, preprocessorColor);
        
        COLOR_RET(TOKEN_ID_DATATYPE_STRUCT_DEF, operatorColor);
        COLOR_RET(TOKEN_ID_DATATYPE_TYPEDEF_DEF, operatorColor);
        COLOR_RET(TOKEN_ID_DATATYPE_CLASS_DEF, operatorColor);
        COLOR_RET(TOKEN_ID_DATATYPE_ENUM_DEF, operatorColor);
        COLOR_RET(TOKEN_ID_MORE, mathColor);
        COLOR_RET(TOKEN_ID_LESS, mathColor);
        
        COLOR_RET(TOKEN_ID_COMMA, tokensColor);
        COLOR_RET(TOKEN_ID_SEMICOLON, tokensColor);
        COLOR_RET(TOKEN_ID_BRACE_OPEN, reservedColor);
        COLOR_RET(TOKEN_ID_BRACE_CLOSE, reservedColor);
        COLOR_RET(TOKEN_ID_PARENTHESE_OPEN, reservedColor);
        COLOR_RET(TOKEN_ID_PARENTHESE_CLOSE, reservedColor);
        COLOR_RET(TOKEN_ID_BRACKET_OPEN, reservedColor);
        COLOR_RET(TOKEN_ID_BRACKET_CLOSE, reservedColor);
        
        COLOR_RET(TOKEN_ID_DATATYPE_USER_STRUCT, userDatatypeColor);
        COLOR_RET(TOKEN_ID_DATATYPE_USER_DATATYPE, userDatatypeColor);
        COLOR_RET(TOKEN_ID_DATATYPE_USER_TYPEDEF, userDatatypeColor);
        COLOR_RET(TOKEN_ID_DATATYPE_USER_CLASS, userDatatypeColor);
        COLOR_RET(TOKEN_ID_DATATYPE_USER_ENUM_VALUE, userDatatypeEnum);
        COLOR_RET(TOKEN_ID_DATATYPE_USER_ENUM, preprocessorDefineColor);
        
        
        COLOR_RET(TOKEN_ID_PREPROCESSOR_DEFINITION, testColor);
        default:{
            AssertA(0, "Unknown mapping for id");
            return vec4i(255);
        }
    }
    
#undef COLOR_RET
    return ApplyAlpha(c, theme);
}

vec4i GetUIColor(Theme *theme, UIElement id){
#define COLOR_RET(i, v) case i : return ApplyAlpha(theme->v, theme);
    switch(id){
        COLOR_RET(UILineNumberBackground, backgroundLineNumbers);
        COLOR_RET(UIBackground, backgroundColor);
        COLOR_RET(UISelectorBackground, selectorBackground);
        COLOR_RET(UISearchBackground, searchBackgroundColor);
        COLOR_RET(UISearchWord, searchWordColor);
        COLOR_RET(UICursorLineHighlight, cursorLineHighlight);
        COLOR_RET(UILineNumberHighlighted, lineNumberHighlitedColor);
        COLOR_RET(UILineNumbers, lineNumberColor);
        COLOR_RET(UIBorder, borderColor);
        COLOR_RET(UICursor, cursorColor);
        COLOR_RET(UIQueryBarCursor, querybarCursorColor);
        COLOR_RET(UIGhostCursor, ghostCursorColor);
        COLOR_RET(UISelectableListBackground, selectableListBackground);
        COLOR_RET(UIScopeLine, scopeLineColor);
        default:{
            AssertA(0, "Unknown mapping for id");
            return vec4i(255);
        }
    }
#undef COLOR_RET
}

vec4f GetUIColorf(Theme *theme, UIElement id){
    vec4i col = GetUIColor(theme, id);
    return vec4f(col.x * kInv255, col.y * kInv255,
                 col.z * kInv255, col.w * kInv255);
}

vec4f GetColorf(Theme *theme, TokenId id){
    vec4i col = GetColor(theme, id);
    return vec4f(col.x * kInv255, col.y * kInv255,
                 col.z * kInv255, col.w * kInv255);
}

vec4f GetNestColorf(Theme *theme, TokenId id, int level){
    vec4i col = GetNestColor(theme, id, level);
    return vec4f(col.x * kInv255, col.y * kInv255,
                 col.z * kInv255, col.w * kInv255);
}

short GetBackTextCount(Theme *theme){
    return theme->backTextCount;
}

Float GetLineBorderWidth(Theme *theme){
    return theme->lineBorderWidth;
}
