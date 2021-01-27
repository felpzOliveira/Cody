#include <theme.h>
#include <utilities.h>

Theme defaultTheme = {
    //.backgroundColor = ColorFromHex(0x161616),
    .backgroundColor = ColorFromHex(0x000000),
    .operatorColor   = ColorFromHex(0xCD950C),
    .datatypeColor   = ColorFromHex(0xCD950C),
    .commentColor    = ColorFromHex(0x7D7D7D),
    .stringColor     = ColorFromHex(0x6B8E23),
    .numberColor     = ColorFromHex(0x6B8E23),
    .reservedColor   = ColorFromHex(0x6B8E23),
    .functionColor   = ColorFromHex(0x6B8E23),
    .includeColor    = ColorFromHex(0x6B8E23),
    .mathColor       = ColorFromHex(0xAB8E23),
    .tokensColor     = ColorFromHex(0xA08563),
    .preprocessorColor = ColorFromHex(0xDAB98F),
};

vec3i GetColor(Theme *theme, TokenId id){
#define COLOR_RET(i, v) case i : return theme->v;
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
        COLOR_RET(TOKEN_ID_NONE, tokensColor);
        COLOR_RET(TOKEN_ID_PREPROCESSOR, preprocessorColor);
        default:{
            AssertA(0, "Unknown mapping for id");
            return vec3i(255);
        }
    }
    
#undef COLOR_RET
}