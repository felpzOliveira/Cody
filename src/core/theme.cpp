#include <theme.h>
#include <utilities.h>

Theme defaultTheme = {
    //.backgroundColor = ColorFromHex(0xFF161616),
    .backgroundColor = ColorFromHex(0xFF000000),
    .operatorColor   = ColorFromHex(0xCCCD950C),
    .datatypeColor   = ColorFromHex(0xCCCD950C),
    .commentColor    = ColorFromHex(0xFF7D7D7D),
    .stringColor     = ColorFromHex(0xFF6B8E23),
    .numberColor     = ColorFromHex(0xFF6B8E23),
    .reservedColor   = ColorFromHex(0xFF6B8E23),
    .functionColor   = ColorFromHex(0xFF6B8E23),
    .includeColor    = ColorFromHex(0xFF6B8E23),
    .mathColor       = ColorFromHex(0xCCAB8E23),
    .tokensColor     = ColorFromHex(0xFFA08563),
    .preprocessorColor = ColorFromHex(0xFFDAB98F),
    .preprocessorDefineColor = ColorFromHex(0xAADAB98F),
    //.preprocessorDefineColor = ColorFromHex(0xFFDD1A01),
    .borderColor = ColorFromHex(0xFFFF7F50),
    
    .testColor = ColorFromHex(0xCCCD950C),
};

static int globalActive = 0;
void SetAlpha(int acitve){
    globalActive = acitve;
}

vec4f GetColorf(Theme *theme, TokenId id){
    vec4i col;
#define COLOR_RET(i, v) case i : col =  theme->v; break;
    switch(id){
        COLOR_RET(TOKEN_ID_OPERATOR, operatorColor);
        COLOR_RET(TOKEN_ID_DATATYPE, datatypeColor);
        COLOR_RET(TOKEN_ID_COMMENT, commentColor);
        COLOR_RET(TOKEN_ID_STRING, stringColor);
        COLOR_RET(TOKEN_ID_NUMBER, numberColor);
        COLOR_RET(TOKEN_ID_RESERVED, reservedColor);
        COLOR_RET(TOKEN_ID_FUNCTION, functionColor);
        COLOR_RET(TOKEN_ID_INCLUDE, includeColor);
        COLOR_RET(TOKEN_ID_MATH, mathColor);
        COLOR_RET(TOKEN_ID_ASTERISK, mathColor);
        COLOR_RET(TOKEN_ID_NONE, tokensColor);
        COLOR_RET(TOKEN_ID_PREPROCESSOR, preprocessorColor);
        COLOR_RET(TOKEN_ID_PREPROCESSOR_DEFINE, preprocessorColor);
        COLOR_RET(TOKEN_ID_PREPROCESSOR_DEFINITION, preprocessorDefineColor);
        COLOR_RET(TOKEN_ID_DATATYPE_STRUCT_DEF, operatorColor);
        COLOR_RET(TOKEN_ID_DATATYPE_TYPEDEF_DEF, operatorColor);
        COLOR_RET(TOKEN_ID_DATATYPE_CLASS_DEF, operatorColor);
        COLOR_RET(TOKEN_ID_DATATYPE_ENUM_DEF, operatorColor);
        COLOR_RET(TOKEN_ID_BRACE_OPEN, tokensColor);
        COLOR_RET(TOKEN_ID_BRACE_CLOSE, tokensColor);
        COLOR_RET(TOKEN_ID_PARENTHESE_OPEN, tokensColor);
        COLOR_RET(TOKEN_ID_PARENTHESE_CLOSE, tokensColor);
        COLOR_RET(TOKEN_ID_BRACKET_OPEN, tokensColor);
        COLOR_RET(TOKEN_ID_BRACKET_CLOSE, tokensColor);
        COLOR_RET(TOKEN_ID_MORE, operatorColor);
        COLOR_RET(TOKEN_ID_LESS, operatorColor);
        
        COLOR_RET(TOKEN_ID_COMMA, tokensColor);
        COLOR_RET(TOKEN_ID_SEMICOLON, tokensColor);
        
        COLOR_RET(TOKEN_ID_DATATYPE_USER_STRUCT, preprocessorDefineColor);
        COLOR_RET(TOKEN_ID_DATATYPE_USER_TYPEDEF, testColor);
        COLOR_RET(TOKEN_ID_DATATYPE_USER_ENUM, preprocessorDefineColor);
        COLOR_RET(TOKEN_ID_DATATYPE_USER_CLASS, testColor);
        default:{
            AssertA(0, "Unknown mapping for id");
            return vec4f(0);
        }
    }
#undef COLOR_RET
    
    vec4f c = vec4f(col.x * kInv255, col.y * kInv255,
                    col.z * kInv255, col.w * kInv255);
    if(globalActive) c.w *= 0.7;
    return c;
}

vec4i GetUIColor(Theme *theme, UIElement id){
#define COLOR_RET(i, v) case i : return theme->v;
    switch(id){
        COLOR_RET(UIBorder, borderColor);
        default:{
            AssertA(0, "Unknown mapping for id");
            return vec4i(255);
        }
    }
#undef COLOR_RET
}

vec4f GetUIColorf(Theme *theme, UIElement id){
    vec4i col;
#define COLOR_RET(i, v) case i : col =  theme->v; break;
    switch(id){
        COLOR_RET(UIBorder, borderColor);
        default:{
            AssertA(0, "Unknown mapping for id");
            return vec4f(1);
        }
    }
#undef COLOR_RET
    
    return vec4f(col.x * kInv255, col.y * kInv255,
                 col.z * kInv255, col.w * kInv255);
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
        COLOR_RET(TOKEN_ID_INCLUDE, includeColor);
        COLOR_RET(TOKEN_ID_MATH, mathColor);
        COLOR_RET(TOKEN_ID_ASTERISK, mathColor);
        COLOR_RET(TOKEN_ID_NONE, tokensColor);
        COLOR_RET(TOKEN_ID_PREPROCESSOR, preprocessorColor);
        COLOR_RET(TOKEN_ID_PREPROCESSOR_DEFINE, preprocessorColor);
        COLOR_RET(TOKEN_ID_PREPROCESSOR_DEFINITION, preprocessorDefineColor);
        COLOR_RET(TOKEN_ID_DATATYPE_STRUCT_DEF, operatorColor);
        COLOR_RET(TOKEN_ID_DATATYPE_TYPEDEF_DEF, operatorColor);
        COLOR_RET(TOKEN_ID_DATATYPE_CLASS_DEF, operatorColor);
        COLOR_RET(TOKEN_ID_DATATYPE_ENUM_DEF, operatorColor);
        COLOR_RET(TOKEN_ID_MORE, operatorColor);
        COLOR_RET(TOKEN_ID_LESS, operatorColor);
        
        COLOR_RET(TOKEN_ID_COMMA, tokensColor);
        COLOR_RET(TOKEN_ID_SEMICOLON, tokensColor);
        COLOR_RET(TOKEN_ID_BRACE_OPEN, tokensColor);
        COLOR_RET(TOKEN_ID_BRACE_CLOSE, tokensColor);
        COLOR_RET(TOKEN_ID_PARENTHESE_OPEN, tokensColor);
        COLOR_RET(TOKEN_ID_PARENTHESE_CLOSE, tokensColor);
        COLOR_RET(TOKEN_ID_BRACKET_OPEN, tokensColor);
        COLOR_RET(TOKEN_ID_BRACKET_CLOSE, tokensColor);
        
        COLOR_RET(TOKEN_ID_DATATYPE_USER_STRUCT, preprocessorDefineColor);
        COLOR_RET(TOKEN_ID_DATATYPE_USER_TYPEDEF, testColor);
        COLOR_RET(TOKEN_ID_DATATYPE_USER_CLASS, testColor);
        COLOR_RET(TOKEN_ID_DATATYPE_USER_ENUM, preprocessorDefineColor);
        default:{
            AssertA(0, "Unknown mapping for id");
            return vec4i(255);
        }
    }
    
#undef COLOR_RET
    
    if(globalActive) c.w *= 0.7;
    return c;
}