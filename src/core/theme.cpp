#include <theme.h>
#include <utilities.h>

Theme themeEverDark = {
    .backgroundColor = ColorFromHex(0xFF0B1210),
    .hoverableItemForegroundColor = ColorFromHex(0xFF30373D),
    .hoverableItemBackgroundColor = ColorFromHex(0xFFA5BF83),
    .selectorBackground = ColorFromHex(0xAA0B1210),
    .searchBackgroundColor = ColorFromHex(0xFFA5BF83),
    .selectableListBackground = ColorFromHex(0xFF0B1210),
    .searchWordColor = ColorFromHex(0xFF30373D),
    .backgroundLineNumbers = ColorFromHex(0xFF0B1210),
    .lineNumberColor = ColorFromHex(0x9955635F),
    .lineNumberHighlitedColor = ColorFromHex(0xFF7F8C84),
    .cursorLineHighlight = ColorFromHex(0xFF0E1614),
    .fileHeaderColor = ColorFromHex(0xCC7AD5A4),
    .operatorColor   = ColorFromHex(0xCCE97C81),

    .datatypeColor   = ColorFromHex(0xCC7AB9B1),
    .commentColor    = ColorFromHex(0xFF5B6660),
    .commentTodoColor = ColorFromHex(0xFFE87C7F),
    .commentNoteColor      = ColorFromHex(0xFF7FA782),
    .commentImportantColor = ColorFromHex(0xFFA7A782),
    .stringColor     = ColorFromHex(0xCC7FBE92),
    .numberColor     = ColorFromHex(0xCCD696B2),
    .reservedColor   = ColorFromHex(0xCCD4C5AA),
    .functionColor   = ColorFromHex(0xCCA5BD82),
    .includeColor    = ColorFromHex(0xCCACB705),
    .mathColor       = ColorFromHex(0xCCDCBA82),
    .tokensColor     = ColorFromHex(0xCCD2C4A9),
    .tokensOverCursorColor = ColorFromHex(0xFF30373D),

    /////////////////////////////////////////////////////
    .preprocessorColor = ColorFromHex(0xCCA6BE82),
    .preprocessorDefineColor = ColorFromHex(0xCC7BB9B2),
    .borderColor = ColorFromHex(0xFFFF7F50),
    /////////////////////////////////////////////////////

    .braces = ColorFromHex(0xFF73B079),
    .cursorColor = ColorFromHex(0xFFD2C4AB),

    .querybarCursorColor = ColorFromHex(0xFFD2C4AB),
    .ghostCursorColor = ColorFromHex(0xFFD2C4AB),
    .parenthesis = {
        ColorFromHex(0xFF73B079),
        ColorFromHex(0xFF73B079),
        ColorFromHex(0xFF73B079),
        ColorFromHex(0xFF73B079),
    },
    .backTextColors = {
        ColorFromHex(0xFF0B1210),
    },

    .userDatatypeColor = ColorFromHex(0xCC7AB9B1),

    ///////////////////////////////////////////////////
    .userDatatypeEnum = ColorFromHex(0xCC7AB9B1),
    ///////////////////////////////////////////////////

    .scopeLineColor = ColorFromHex(0xAA7B8680),
    .scrollbarColor = ColorFromHex(0xFF0E1614),
    .querybarTypeLineColor = ColorFromHex(0xFF221409),
    .selectorLoadedColor = ColorFromHex(0xFFE99779),
    .userDefineColor = ColorFromHex(0xCC7AB9B1),

    .backTextCount = 0,
    .lineBorderWidth = 0,
    .alphaDimm = 0,
    .dynamicCursor = false,
    .borderWidth = 3,
    .pasteColor = ColorFromHex(0xffffddee),
    .dbgArrowColor = ColorFromHex(0xffffffff),
    .dbgLinehighlightColor = ColorFromHex(0xFF232340),
    .mouseSelectionColor = ColorFromHex(0xFF0E1614),
    .isLight = false,
    .renderLineNumberDiff = true,
    .visuals = {
        .brightness = 0.05f,
        .saturation = 1.2f,
        .contrast = 2.5f,
    },
};

Theme themeToxi = {
    .backgroundColor = ColorFromHex(0xFF0B1210),
    .hoverableItemForegroundColor = ColorFromHex(0xFFFDFB00),
    .hoverableItemBackgroundColor = ColorFromHex(0xFF441EA4),
    .selectorBackground = ColorFromHex(0xFF0B1210),
    .searchBackgroundColor = ColorFromHex(0xFF441EA4),
    .selectableListBackground = ColorFromHex(0xFF0E1C1E),
    .searchWordColor = ColorFromHex(0xFF96147E),
    .backgroundLineNumbers = ColorFromHex(0xFF0B1210),
    .lineNumberColor = ColorFromHex(0x000B1210),
    .lineNumberHighlitedColor = ColorFromHex(0xFF14BBCB),
    .cursorLineHighlight = ColorFromHex(0xFF0B1210),
    .fileHeaderColor = ColorFromHex(0xCCBFBFBF),
    .operatorColor   = ColorFromHex(0xCCFDFB00),

    .datatypeColor   = ColorFromHex(0xCCE5071F),
    .commentColor    = ColorFromHex(0xFF707273),
    .commentTodoColor = ColorFromHex(0xFFE90329),
    .commentNoteColor = ColorFromHex(0xFFE90329),
    .commentImportantColor = ColorFromHex(0xFFE90329),
    .stringColor     = ColorFromHex(0xCC48D867),
    .numberColor     = ColorFromHex(0xCC97F0CD),
    .reservedColor   = ColorFromHex(0xCC73B079),
    .functionColor   = ColorFromHex(0xCCC89C3D),
    .includeColor    = ColorFromHex(0xCCACB705),
    .mathColor       = ColorFromHex(0xCCE3B643),
    .tokensColor     = ColorFromHex(0xCC7AD5A4),
    .tokensOverCursorColor = ColorFromHex(0xFF0B1210),

    /////////////////////////////////////////////////////
    .preprocessorColor = ColorFromHex(0xCCF10143),
    .preprocessorDefineColor = ColorFromHex(0xCCF10143),
    .borderColor = ColorFromHex(0xFFFF7F50),
    /////////////////////////////////////////////////////

    .braces = ColorFromHex(0xFF73B079),
    .cursorColor = ColorFromHex(0xFF75A36A),

    .querybarCursorColor = ColorFromHex(0xFF75A36A),
    .ghostCursorColor = ColorFromHex(0xFF75A36A),
    .parenthesis = {
        ColorFromHex(0xFF73B079),
        ColorFromHex(0xFF73B079),
        ColorFromHex(0xFF73B079),
        ColorFromHex(0xFF73B079),
    },
    .backTextColors = {
        ColorFromHex(0xFF0B1210),
    },

    .userDatatypeColor = ColorFromHex(0xCCE5071F),

    ///////////////////////////////////////////////////
    .userDatatypeEnum = ColorFromHex(0xCC5793A4),
    ///////////////////////////////////////////////////

    .scopeLineColor = ColorFromHex(0xAAAAAAAA),
    .scrollbarColor = ColorFromHex(0xFF1A1A1A),
    .querybarTypeLineColor = ColorFromHex(0xFF330d0d),
    .selectorLoadedColor = ColorFromHex(0xFFF5BD96),
    .userDefineColor = ColorFromHex(0xCCE5071F),

    .backTextCount = 0,
    .lineBorderWidth = 0,
    .alphaDimm = 0,
    .dynamicCursor = false,
    .borderWidth = 3,
    .pasteColor = ColorFromHex(0xffffddee),
    .dbgArrowColor = ColorFromHex(0xffffffff),
    .dbgLinehighlightColor = ColorFromHex(0xFF232340),
    .mouseSelectionColor = ColorFromHex(0xFF232333),
    .isLight = false,
    .renderLineNumberDiff = false,
    .visuals = {
        .brightness = 0.05f,
        .saturation = 1.0f,
        .contrast = 1.75f,
    },
};

Theme themeGrayVim = {
    .backgroundColor = ColorFromHex(0xFF191819),
    .hoverableItemForegroundColor = ColorFromHex(0xFF72ae55),
    .hoverableItemBackgroundColor = ColorFromHex(0xFF232224),
    .selectorBackground = ColorFromHex(0xFF191819),
    .searchBackgroundColor = ColorFromHex(0xFF232224),
    .selectableListBackground = ColorFromHex(0xFF1d1c1d),
    .searchWordColor = ColorFromHex(0xFF72ae55),
    .backgroundLineNumbers = ColorFromHex(0xFF191819),
    .lineNumberColor = ColorFromHex(0xFF7F6E63),
    .lineNumberHighlitedColor = ColorFromHex(0xFFFFB815),
    .cursorLineHighlight = ColorFromHex(0xFF191819),
    .fileHeaderColor = ColorFromHex(0xFFcf4341),
    .operatorColor   = ColorFromHex(0xFFFFB815),

    .datatypeColor   = ColorFromHex(0xFFcf4341),
    .commentColor    = ColorFromHex(0xFFbc8b53),
    .commentTodoColor = ColorFromHex(0xFFbc8b53),
    .commentNoteColor = ColorFromHex(0xFFbc8b53),
    .commentImportantColor = ColorFromHex(0xFFbc8b53),
    .stringColor     = ColorFromHex(0xCC8bb92d),
    .numberColor     = ColorFromHex(0xCC8bb92d),
    .reservedColor   = ColorFromHex(0xFFF2DAB1),
    /////////////////////////////////////////////
    .functionColor   = ColorFromHex(0xFFF2DAB1),
    .includeColor    = ColorFromHex(0xCC8bb92d),
    /////////////////////////////////////////////
    .mathColor       = ColorFromHex(0xFFF2DAB1),
    .tokensColor     = ColorFromHex(0xFFF2DAB1),
    .tokensOverCursorColor = ColorFromHex(0xFF191819),

    /////////////////////////////////////////////////////
    .preprocessorColor = ColorFromHex(0xCCfff0ba),
    .preprocessorDefineColor = ColorFromHex(0xCCfff0ba),
    .borderColor = ColorFromHex(0xFFFF7F50),
    /////////////////////////////////////////////////////

    .braces = ColorFromHex(0xFFF2DAB1),
    .cursorColor = ColorFromHex(0xFFffd547),

    .querybarCursorColor = ColorFromHex(0xFFFFB815),
    .ghostCursorColor = ColorFromHex(0xFFFFB815),
    .parenthesis = {
        ColorFromHex(0xFFFFB815),
        ColorFromHex(0xFFFFB815),
        ColorFromHex(0xFFFFB815),
        ColorFromHex(0xFFFFB815),
    },
    .backTextColors = {
        ColorFromHex(0xFF191819),
        ColorFromHex(0xFF191819),
        ColorFromHex(0xFF191819),
        ColorFromHex(0xFF191819),
    },

    .userDatatypeColor = ColorFromHex(0xFFcf4341),

    ///////////////////////////////////////////////////
    .userDatatypeEnum = ColorFromHex(0xCCffffe1),
    ///////////////////////////////////////////////////

    .scopeLineColor = ColorFromHex(0xAAAAAAAA),
    .scrollbarColor = ColorFromHex(0xFF232123),
    .querybarTypeLineColor = ColorFromHex(0xFF461213),
    .selectorLoadedColor = ColorFromHex(0xFFCB9401),
    .userDefineColor = ColorFromHex(0xFFF2DAB1),

    .backTextCount = 4,
    .lineBorderWidth = 0,
    .alphaDimm = 0,
    .dynamicCursor = false,
    .borderWidth = 3,
    .pasteColor = ColorFromHex(0xffffddee),
    .dbgArrowColor = ColorFromHex(0xffffffff),
    .dbgLinehighlightColor = ColorFromHex(0xFF232340),
    .mouseSelectionColor = ColorFromHex(0xFF232123),
    .isLight = false,
    .renderLineNumberDiff = false,
    .visuals = {
        .brightness = 0.05f,
        .saturation = 1.0f,
        .contrast = 1.4f,
    },
};

Theme themeDarkVim = {
    .backgroundColor = ColorFromHex(0xFF0A0613),
    .hoverableItemForegroundColor = ColorFromHex(0xCCD9DFE3),
    .hoverableItemBackgroundColor = ColorFromHex(0xFF2F2E3E),
    .selectorBackground = ColorFromHex(0xFF060816),
    .searchBackgroundColor = ColorFromHex(0xFFFFB815),
    .selectableListBackground = ColorFromHex(0xFF101220),
    .searchWordColor = ColorFromHex(0xFF0A0613),
    .backgroundLineNumbers = ColorFromHex(0xFF0A0613),
    .lineNumberColor = ColorFromHex(0xFF7F6E63),
    .lineNumberHighlitedColor = ColorFromHex(0xFFFFB815),
    .cursorLineHighlight = ColorFromHex(0xFF0A0613),
    .fileHeaderColor = ColorFromHex(0xCC76A698),
    .operatorColor   = ColorFromHex(0xCCFF7300),

    .datatypeColor   = ColorFromHex(0xCC76A698),
    .commentColor    = ColorFromHex(0xCC9F947D),
    .commentTodoColor = ColorFromHex(0xFFA8181F),
    .commentNoteColor = ColorFromHex(0XFF18A81F),
    .commentImportantColor = ColorFromHex(0xFFA8A81F),
    .stringColor     = ColorFromHex(0xFFF2DAB1),
    .numberColor     = ColorFromHex(0xFFFF7400),
    .reservedColor   = ColorFromHex(0xFFBBBA0F),
    /////////////////////////////////////////////
    .functionColor   = ColorFromHex(0xCC78C9FB),
    .includeColor    = ColorFromHex(0xCC98DCB2),
    /////////////////////////////////////////////
    .mathColor       = ColorFromHex(0xCCACC4D3),
    .tokensColor     = ColorFromHex(0xFFF2DAB1),
    .tokensOverCursorColor = ColorFromHex(0xFF0A0613),

    /////////////////////////////////////////////////////
    .preprocessorColor = ColorFromHex(0xCCfff0ba),
    .preprocessorDefineColor = ColorFromHex(0xCCfff0ba),
    .borderColor = ColorFromHex(0xFFFF7F50),
    /////////////////////////////////////////////////////

    .braces = ColorFromHex(0xFFE6977D),
    .cursorColor = ColorFromHex(0xFFFFFFFF),

    .querybarCursorColor = ColorFromHex(0xFFFF4040),
    .ghostCursorColor = ColorFromHex(0xFFDADAE8),
    .parenthesis = {
        ColorFromHex(0xFFFFB815),
        ColorFromHex(0xFFFFB815),
        ColorFromHex(0xFFFFB815),
        ColorFromHex(0xFFFFB815),
    },
    .backTextColors = {
        ColorFromHex(0xFF0A0613),
        ColorFromHex(0xFF0A0613),
        ColorFromHex(0xFF0A0613),
        ColorFromHex(0xFF0A0613),
    },

    .userDatatypeColor = ColorFromHex(0xCC76A698),

    ///////////////////////////////////////////////////
    .userDatatypeEnum = ColorFromHex(0xCCffffe1),
    ///////////////////////////////////////////////////

    .scopeLineColor = ColorFromHex(0xAAAAAAAA),
    .scrollbarColor = ColorFromHex(0xff25253f),
    .querybarTypeLineColor = ColorFromHex(0xFF330d0d),
    .selectorLoadedColor = ColorFromHex(0xFFCB9401),
    .userDefineColor = ColorFromHex(0xCCff628a),

    .backTextCount = 4,
    .lineBorderWidth = 0,
    .alphaDimm = 0,
    .dynamicCursor = false,
    .borderWidth = 3,
    .pasteColor = ColorFromHex(0xffffddee),
    .dbgArrowColor = ColorFromHex(0xffffffff),
    .dbgLinehighlightColor = ColorFromHex(0xFF232340),
    .mouseSelectionColor = ColorFromHex(0xFF232333),
    .isLight = false,
    .renderLineNumberDiff = false,
    .visuals = {
        .brightness = 0.05f,
        .saturation = 1.0f,
        .contrast = 1.2f,
    },
};

Theme themeCatppuccin = {
    .backgroundColor = ColorFromHex(0xFF111121),
    .hoverableItemForegroundColor = ColorFromHex(0xCCD9DFE3),
    .hoverableItemBackgroundColor = ColorFromHex(0xFF2F2E3E),
    .selectorBackground = ColorFromHex(0xFF111121),
    .searchBackgroundColor = ColorFromHex(0xFF383747),
    .selectableListBackground = ColorFromHex(0xFF1C1C2C),
    .searchWordColor = ColorFromHex(0xFFF28FAD),
    .backgroundLineNumbers = ColorFromHex(0xFF111121),
    .lineNumberColor = ColorFromHex(0xFF42404E),
    .lineNumberHighlitedColor = ColorFromHex(0xFFD9E0EC),
    .cursorLineHighlight = ColorFromHex(0xFF151525),
    .fileHeaderColor = ColorFromHex(0xCCF0899D),
    .operatorColor   = ColorFromHex(0xCCF7B472),

    .datatypeColor   = ColorFromHex(0xCCF0899D),
    .commentColor    = ColorFromHex(0xFF69676A),
    .commentTodoColor = ColorFromHex(0xFFA8181F),
    .commentNoteColor = ColorFromHex(0XFF18A81F),
    .commentImportantColor = ColorFromHex(0xFFA8A81F),
    .stringColor     = ColorFromHex(0xCC98DCB2),
    .numberColor     = ColorFromHex(0xCC88DBE7),
    .reservedColor   = ColorFromHex(0xCCd0ad81),
    .functionColor   = ColorFromHex(0xCC78C9FB),
    .includeColor    = ColorFromHex(0xCC98DCB2),
    .mathColor       = ColorFromHex(0xCCACC4D3),
    .tokensColor     = ColorFromHex(0xCCACC4D3),
    .tokensOverCursorColor = ColorFromHex(0xFF262D49),

    /////////////////////////////////////////////////////
    .preprocessorColor = ColorFromHex(0xCCfff0ba),
    .preprocessorDefineColor = ColorFromHex(0xCCfff0ba),
    .borderColor = ColorFromHex(0xFFFF7F50),
    /////////////////////////////////////////////////////

    .braces = ColorFromHex(0xFFE6977D),
    .cursorColor = ColorFromHex(0xFFDADAE8),

    .querybarCursorColor = ColorFromHex(0xFFFF4040),
    .ghostCursorColor = ColorFromHex(0xFFDADAE8),
    .parenthesis = {
        ColorFromHex(0xFFFF0000),
        ColorFromHex(0xFF00FF00),
        ColorFromHex(0xFF0000FF),
        ColorFromHex(0xFFAAAA00),
    },
    .backTextColors = {
        ColorFromHex(0xFF111121),
    },

    .userDatatypeColor = ColorFromHex(0xCCDF8E1D),

    ///////////////////////////////////////////////////
    .userDatatypeEnum = ColorFromHex(0xCCffffe1),
    ///////////////////////////////////////////////////

    .scopeLineColor = ColorFromHex(0xAAAAAAAA),
    .scrollbarColor = ColorFromHex(0xff25253f),
    .querybarTypeLineColor = ColorFromHex(0xFF330d0d),
    .selectorLoadedColor = ColorFromHex(0xFFF5BD96),
    .userDefineColor = ColorFromHex(0xCCff628a),

    .backTextCount = 1,
    .lineBorderWidth = 0,
    .alphaDimm = 0,
    .dynamicCursor = false,
    .borderWidth = 3,
    .pasteColor = ColorFromHex(0xffffddee),
    .dbgArrowColor = ColorFromHex(0xffffffff),
    .dbgLinehighlightColor = ColorFromHex(0xFF232340),
    .mouseSelectionColor = ColorFromHex(0xFF232333),
    .isLight = false,
    .renderLineNumberDiff = false,
    .visuals = {
        .brightness = 0.05f,
        .saturation = 1.0f,
        .contrast = 1.75f,
    },
};

Theme themeTerminal = {
    .backgroundColor = ColorFromHex(0xff2C001e),
    .hoverableItemForegroundColor = ColorFromHex(0xffd7d7d3),
    .hoverableItemBackgroundColor = ColorFromHex(0xff611449),
    .selectorBackground = ColorFromHex(0xff2C001e),

    .searchBackgroundColor = ColorFromHex(0xff87ffaf),
    .selectableListBackground = ColorFromHex(0xff340b27),
    .searchWordColor = ColorFromHex(0xff380c2a),
    .backgroundLineNumbers = ColorFromHex(0xff2C001e),
    .lineNumberColor = ColorFromHex(0xfffce94f),
    .lineNumberHighlitedColor = ColorFromHex(0xfffce94f),
    .cursorLineHighlight = ColorFromHex(0xff2e084a),
    .fileHeaderColor = ColorFromHex(0xff87ffaf),

    .operatorColor   = ColorFromHex(0xfffce94f),
    .datatypeColor   = ColorFromHex(0xff87ffaf),
    .commentColor    = ColorFromHex(0xff34e2e2),
    .commentTodoColor = ColorFromHex(0xffc7ff00),
    .commentNoteColor = ColorFromHex(0xffc7ff00),
    .commentImportantColor = ColorFromHex(0xffc7ff00),
    .stringColor     = ColorFromHex(0xffad7fa6),
    .numberColor     = ColorFromHex(0xffad7fa8),
    .reservedColor   = ColorFromHex(0xffad7fa8),
    .functionColor   = ColorFromHex(0xffCFB4C2),
    .includeColor    = ColorFromHex(0xffad7fa8),

    .mathColor       = ColorFromHex(0xffffd7d7),

    .tokensColor     = ColorFromHex(0xffd7d7d7),
    .tokensOverCursorColor = ColorFromHex(0xff2C001e),

    .preprocessorColor = ColorFromHex(0xff5fd7fc),
    .preprocessorDefineColor = ColorFromHex(0xff5fd7fc),

    .borderColor = ColorFromHex(0xFFFF7F50),
    .braces = ColorFromHex(0xCC960d01),
    .cursorColor = ColorFromHex(0xffd7d7d7),
    .querybarCursorColor = ColorFromHex(0xffd7d7d7),
    .ghostCursorColor = ColorFromHex(0xffd7d7d7),
    .parenthesis = {
        ColorFromHex(0xFFFF0000),
        ColorFromHex(0xFF00FF00),
        ColorFromHex(0xFF0000FF),
        ColorFromHex(0xFFAAAA00),
    },
    .backTextColors = {
        ColorFromHex(0xff2C001e),
    },

    .userDatatypeColor = ColorFromHex(0xff7ceba1),
    .userDatatypeEnum = ColorFromHex(0xffffd7d7),
    .scopeLineColor = ColorFromHex(0x66666666),
    .scrollbarColor = ColorFromHex(0xff2C001e),
    .querybarTypeLineColor = ColorFromHex(0xff340b27),
    .selectorLoadedColor = ColorFromHex(0xFF458588),
    .userDefineColor = ColorFromHex(0xff5fd7fc),
    .backTextCount = 1,
    .lineBorderWidth = 0,
    .alphaDimm = 0,
    .dynamicCursor = true,
    .borderWidth = 3,
    .pasteColor = ColorFromHex(0xffffddee),
    .dbgArrowColor = ColorFromHex(0xffffffff),
    .dbgLinehighlightColor = ColorFromHex(0xff2e084a),
    .mouseSelectionColor = ColorFromHex(0xff2e084a),
    .isLight = false,
    .renderLineNumberDiff = false,
    .visuals = {
        .brightness = 0.15f,
        .saturation = 1.0f,
        .contrast = 1.0f,
    },
};

Theme themeGruvboxLight = {
    .backgroundColor = ColorFromHex(0xFFfbf1c7),
    .hoverableItemForegroundColor = ColorFromHex(0xff447a59),
    .hoverableItemBackgroundColor = ColorFromHex(0xFFebdbb2),
    .selectorBackground = ColorFromHex(0xFFfbf1c7),

    .searchBackgroundColor = ColorFromHex(0xff6a191d),
    .selectableListBackground = ColorFromHex(0xFFfbf1c7),
    .searchWordColor = ColorFromHex(0xCCd5c4a1),
    .backgroundLineNumbers = ColorFromHex(0xFFfbf1c7),
    .lineNumberColor = ColorFromHex(0xFF928374),
    .lineNumberHighlitedColor = ColorFromHex(0xFF180f58),
    //.cursorLineHighlight = ColorFromHex(0xFFebdbb2),
    .cursorLineHighlight = ColorFromHex(0xFFE0E1BD),
    .fileHeaderColor = ColorFromHex(0xFF9b0511),

    .operatorColor   = ColorFromHex(0xCC960d01),
    .datatypeColor   = ColorFromHex(0xFF9b0511),
    .commentColor    = ColorFromHex(0xFFA29385),
    .commentTodoColor = ColorFromHex(0xFF00557f),
    .commentNoteColor = ColorFromHex(0xFF00557f),
    .commentImportantColor = ColorFromHex(0xFF00557f),
    .stringColor     = ColorFromHex(0xff124b28),
    .numberColor     = ColorFromHex(0xFF116677),
    .reservedColor   = ColorFromHex(0xFF116677),
    .functionColor   = ColorFromHex(0xFF00557f),
    .includeColor    = ColorFromHex(0xFF022b08),

    .mathColor       = ColorFromHex(0xFF9d0006),

    .tokensColor     = ColorFromHex(0xFF3c3836),
    .tokensOverCursorColor = ColorFromHex(0x88fbf1c7),

    .preprocessorColor = ColorFromHex(0xFF00476d),
    .preprocessorDefineColor = ColorFromHex(0xFF00476d),

    .borderColor = ColorFromHex(0xFFFF7F50),
    .braces = ColorFromHex(0xCC960d01),
    .cursorColor = ColorFromHex(0xFF928375),
    .querybarCursorColor = ColorFromHex(0xFF928375),
    .ghostCursorColor = ColorFromHex(0xFF5B4D3C),
    .parenthesis = {
        ColorFromHex(0xFFFF0000),
        ColorFromHex(0xFF00FF00),
        ColorFromHex(0xFF0000FF),
        ColorFromHex(0xFFAAAA00),
    },
    .backTextColors = {
        ColorFromHex(0xFFfbf1c7),
    },

    .userDatatypeColor = ColorFromHex(0xFF3e1856),
    .userDatatypeEnum = ColorFromHex(0xFF6b2f55),
    .scopeLineColor = ColorFromHex(0x66666666),
    .scrollbarColor = ColorFromHex(0xffebdab4),
    .querybarTypeLineColor = ColorFromHex(0xFFd5c4a3),
    .selectorLoadedColor = ColorFromHex(0xFF458588),
    .userDefineColor = ColorFromHex(0xff00476d),
    .backTextCount = 1,
    .lineBorderWidth = 0,
    .alphaDimm = 0,
    .dynamicCursor = false,
    .borderWidth = 3,
    .pasteColor = ColorFromHex(0xffffddee),
    .dbgArrowColor = ColorFromHex(0xff000000),
    .dbgLinehighlightColor = ColorFromHex(0xFFfbdbb2),
    .mouseSelectionColor = ColorFromHex(0xFFE0E1BD),
    .isLight = true,
    .renderLineNumberDiff = false,
    .visuals = {
        .brightness = 0.02f,
        .saturation = 1.0f,
        .contrast = 2.3f,
    },
};

Theme themeDracula = {
    .backgroundColor = ColorFromHex(0xff192020),
    .hoverableItemForegroundColor = ColorFromHex(0xCCfffc7f),
    .hoverableItemBackgroundColor = ColorFromHex(0xFF44475a),
    .selectorBackground = ColorFromHex(0xff192020),
    .searchBackgroundColor = ColorFromHex(0xFF44475a),
    .selectableListBackground = ColorFromHex(0xff192020),
    .searchWordColor = ColorFromHex(0xCC8e8620),
    .backgroundLineNumbers = ColorFromHex(0xff192020),
    .lineNumberColor = ColorFromHex(0xFF786fa8),
    .lineNumberHighlitedColor = ColorFromHex(0xFFd8cff8),
    .cursorLineHighlight = ColorFromHex(0xff151616),
    .fileHeaderColor = ColorFromHex(0xCCf67397),
    .operatorColor   = ColorFromHex(0xCCD38545),
    .datatypeColor   = ColorFromHex(0xCCff79c6),
    .commentColor    = ColorFromHex(0xFF6272a4),
    .commentTodoColor = ColorFromHex(0xFF50fa7b),
    .commentNoteColor = ColorFromHex(0xFF50fa7b),
    .commentImportantColor = ColorFromHex(0xFF50fa7b),
    .stringColor     = ColorFromHex(0xFFf1fa8c),
    .numberColor     = ColorFromHex(0xFF6B8E23),
    .reservedColor   = ColorFromHex(0xFF6B8E23),
    .functionColor   = ColorFromHex(0xFF50fa7b),
    .includeColor    = ColorFromHex(0xFFf1fa8c),
    .mathColor       = ColorFromHex(0xFFD2D2D3),
    .tokensColor     = ColorFromHex(0xFFf8f8f2),
    .tokensOverCursorColor = ColorFromHex(0xFF000000),
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
        ColorFromHex(0xff192020),
        ColorFromHex(0xff192020),
        ColorFromHex(0xff192020),
        ColorFromHex(0xff192020),
    },
    .userDatatypeColor = ColorFromHex(0xCC8EE7BF),
    .userDatatypeEnum = ColorFromHex(0xCCd8ddad),
    .scopeLineColor = ColorFromHex(0xAAAAAAAA),
    .scrollbarColor = ColorFromHex(0xff353f25),
    .querybarTypeLineColor = ColorFromHex(0xFF330d0d),
    .selectorLoadedColor = ColorFromHex(0xFFCB9401),
    .userDefineColor = ColorFromHex(0xCCff5555),
    .backTextCount = 4,
    .lineBorderWidth = 0,
    .alphaDimm = 0,
    .dynamicCursor = false,
    .borderWidth = 3,
    .pasteColor = ColorFromHex(0xffffddee),
    .dbgArrowColor = ColorFromHex(0xffffffff),
    .dbgLinehighlightColor = ColorFromHex(0xFF44476a),
    .mouseSelectionColor = ColorFromHex(0xff151616),
    .isLight = false,
    .renderLineNumberDiff = false,
    .visuals = {
        .brightness = 0.05f,
        .saturation = 1.0f,
        .contrast = 1.0f,
    },
};

Theme themeYavid = {
    .backgroundColor = ColorFromHex(0xFF16191C),
    .hoverableItemForegroundColor = ColorFromHex(0xCCfffc7f),
    .hoverableItemBackgroundColor = ColorFromHex(0xFF272729),
    .selectorBackground = ColorFromHex(0xFF17171D),
    .searchBackgroundColor = ColorFromHex(0xff87ffaf),
    .selectableListBackground = ColorFromHex(0xFF13151a),
    .searchWordColor = ColorFromHex(0xFF16191C),
    .backgroundLineNumbers = ColorFromHex(0xFF16191C),
    .lineNumberColor = ColorFromHex(0xFF4C4D4E),
    .lineNumberHighlitedColor = ColorFromHex(0xFF858688),
    .cursorLineHighlight = ColorFromHex(0xFF272729),
    .fileHeaderColor = ColorFromHex(0xCC719FAE),
    .operatorColor   = ColorFromHex(0xCCD38545),
    .datatypeColor   = ColorFromHex(0xCC719FAE),
    .commentColor    = ColorFromHex(0xFF7D7D7D),
    .commentTodoColor = ColorFromHex(0xFFA8181F),
    .commentNoteColor = ColorFromHex(0XFF18A81F),
    .commentImportantColor = ColorFromHex(0xFFA8A81F),
    //.commentTodoColor = ColorFromHex(0xFFDFDA77),
    //.commentNoteColor = ColorFromHex(0xFFDFDA77),
    .stringColor     = ColorFromHex(0xFF6B8E23),
    .numberColor     = ColorFromHex(0xFF6B8E23),
    .reservedColor   = ColorFromHex(0xFF6B8E23),
    .functionColor   = ColorFromHex(0xFFDFDA77),
    .includeColor    = ColorFromHex(0xFF6B8E23),
    .mathColor       = ColorFromHex(0xFFD2D2D3),
    .tokensColor     = ColorFromHex(0xFFD2D2D3),
    .tokensOverCursorColor = ColorFromHex(0xFF000000),
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
        //ColorFromHex(0xFF16191C),
        ColorFromHex(0xFF202020),
    },
    .userDatatypeColor = ColorFromHex(0xCC8EE7BF),
    .userDatatypeEnum = ColorFromHex(0xCCd8ddad),
    .scopeLineColor = ColorFromHex(0xAAAAAAAA),
    .scrollbarColor = ColorFromHex(0xff20232c),
    .querybarTypeLineColor = ColorFromHex(0xFF323744),
    .selectorLoadedColor = ColorFromHex(0xFFCB9401),
    .userDefineColor = ColorFromHex(0xCCCD950C),
    .backTextCount = 1,
    .lineBorderWidth = 0,
    .alphaDimm = 0,
    .dynamicCursor = false,
    .borderWidth = 3,
    .pasteColor = ColorFromHex(0xffffddee),
    .dbgArrowColor = ColorFromHex(0xffffffff),
    .dbgLinehighlightColor = ColorFromHex(0xFF272739),
    .mouseSelectionColor = ColorFromHex(0xFF272729),
    .isLight = false,
    .renderLineNumberDiff = false,
    .visuals = {
        .brightness = 0.05f,
        .saturation = 1.0f,
        .contrast = 1.0f,
    },
};

Theme themeRadical = {
    .backgroundColor = ColorFromHex(0xFF090817),
    .hoverableItemForegroundColor = ColorFromHex(0xffa2fa9f),
    .hoverableItemBackgroundColor = ColorFromHex(0xFF241630),
    .selectorBackground = ColorFromHex(0xFF080716),
    .searchBackgroundColor = ColorFromHex(0xFF342640),

    .selectableListBackground = ColorFromHex(0x082f317c),

    .searchWordColor = ColorFromHex(0xCCce0b95),
    .backgroundLineNumbers = ColorFromHex(0xFF090817),
    .lineNumberColor = ColorFromHex(0xFF767585),
    .lineNumberHighlitedColor = ColorFromHex(0xFFD9E0EC),
    .cursorLineHighlight = ColorFromHex(0xFF191124),
    .fileHeaderColor = ColorFromHex(0xCCf67397),

    .operatorColor = ColorFromHex(0xCCc7ff00),

    .datatypeColor   = ColorFromHex(0xCCf67397),
    .commentColor    = ColorFromHex(0x48cff0e9),
    .commentTodoColor = ColorFromHex(0xCCc7ff00),
    .commentNoteColor = ColorFromHex(0xCCc7ff00),
    .commentImportantColor = ColorFromHex(0xCCc7ff00),
    .stringColor     = ColorFromHex(0xCCbaf7fc),
    .numberColor     = ColorFromHex(0xCCfffc7f),
    .reservedColor   = ColorFromHex(0xCCf862b9),
    .functionColor   = ColorFromHex(0xFF54757c),
    .includeColor    = ColorFromHex(0xCCbaf7fc),
    .mathColor       = ColorFromHex(0xCCff5395),
    .tokensColor     = ColorFromHex(0xCCcbfff2),
    .tokensOverCursorColor = ColorFromHex(0xFF000000),
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
        ColorFromHex(0xff0f091c),
    },

    .userDatatypeColor = ColorFromHex(0xCCf67397),
    .userDatatypeEnum = ColorFromHex(0xCCd8ddad),
    .scopeLineColor = ColorFromHex(0xAAd5348f),
    .scrollbarColor = ColorFromHex(0xff353f25),
    .querybarTypeLineColor = ColorFromHex(0xFF330d0d),
    .selectorLoadedColor = ColorFromHex(0xFFCB9401),
    .userDefineColor = ColorFromHex(0xCCfa62b9),
    .backTextCount = 1,
    .lineBorderWidth = 0,
    .alphaDimm = 0,
    .dynamicCursor = false,
    .borderWidth = 3,
    .pasteColor = ColorFromHex(0xffffddee),
    .dbgArrowColor = ColorFromHex(0xffffffff),
    .dbgLinehighlightColor = ColorFromHex(0xFF241640),
    .mouseSelectionColor = ColorFromHex(0xFF232333),
    .isLight = false,
    .renderLineNumberDiff = false,
    .visuals = {
        .brightness = 0.05f,
        .saturation = 1.0f,
        .contrast = 1.5f,
    },
};

Theme theme4coderAlt = {
    .backgroundColor = ColorFromHex(0xff0c0c0c),
    .hoverableItemForegroundColor = ColorFromHex(0xCCfffc7f),
    .hoverableItemBackgroundColor = ColorFromHex(0xff2d3640),
    .selectorBackground = ColorFromHex(0xff0c0c0c),
    .searchBackgroundColor = ColorFromHex(0xff315268),
    .selectableListBackground = ColorFromHex(0xff0f0f0f),
    .searchWordColor = ColorFromHex(0xFF72ae55),
    .backgroundLineNumbers = ColorFromHex(0xff0c0c0c),
    .lineNumberColor = ColorFromHex(0xFF4C4D4E),
    .lineNumberHighlitedColor = ColorFromHex(0xFF7a7c7d),
    .cursorLineHighlight = ColorFromHex(0xff0f0f0f),
    .fileHeaderColor = ColorFromHex(0xCCffc110),
    .operatorColor   = ColorFromHex(0xffac7b0b),

    .datatypeColor   = ColorFromHex(0xCCffc110),
    .commentColor    = ColorFromHex(0xff686868),
    .commentTodoColor = ColorFromHex(0xFFA00000),
    .commentNoteColor = ColorFromHex(0xFF00A000),
    .commentImportantColor = ColorFromHex(0xFFA0A000),
    .stringColor     = ColorFromHex(0xff6b8e23),
    .numberColor     = ColorFromHex(0xff6b8e23),
    .reservedColor   = ColorFromHex(0xCCd0ad81),
    .functionColor   = ColorFromHex(0xffcc5735),
    .includeColor    = ColorFromHex(0xffdab98f),
    .mathColor       = ColorFromHex(0xffa08563),
    .tokensColor     = ColorFromHex(0xffa08563),
    .tokensOverCursorColor = ColorFromHex(0xFF0C0C0C),
    .preprocessorColor = ColorFromHex(0xffdab98f),
    .preprocessorDefineColor = ColorFromHex(0xff478980),

    .borderColor = ColorFromHex(0xFFFF7F50),
    .braces = ColorFromHex(0xFF00FFFF),
    .cursorColor = ColorFromHex(0xFFEE7700),

    .querybarCursorColor = ColorFromHex(0xFFEE7700),
    .ghostCursorColor = ColorFromHex(0xFF5B4D3C),
    .parenthesis = {
        ColorFromHex(0xFFFF0000),
        ColorFromHex(0xFF00FF00),
        ColorFromHex(0xFF0000FF),
        ColorFromHex(0xFFAAAA00),
    },
    .backTextColors = {
        ColorFromHex(0xff0c0c0c),
        ColorFromHex(0xff0c0c0c),
        ColorFromHex(0xff0c0c0c),
        ColorFromHex(0xff0c0c0c),
    },

    .userDatatypeColor = ColorFromHex(0xCCffc110),
    .userDatatypeEnum = ColorFromHex(0xff478980),

    .scopeLineColor = ColorFromHex(0xAAAAAAAA),
    .scrollbarColor = ColorFromHex(0xff353f25),
    .querybarTypeLineColor = ColorFromHex(0xFF330d0d),
    .selectorLoadedColor = ColorFromHex(0xFFCB9401),
    .userDefineColor = ColorFromHex(0xff478980),

    .backTextCount = 4,
    .lineBorderWidth = 0,
    .alphaDimm = 0,
    .dynamicCursor = false,
    .borderWidth = 3,
    .pasteColor = ColorFromHex(0xffffddee),
    .dbgArrowColor = ColorFromHex(0xffffffff),
    .dbgLinehighlightColor = ColorFromHex(0xFF232340),
    .mouseSelectionColor = ColorFromHex(0xff0f0f0f),
    .isLight = false,
    .renderLineNumberDiff = false,
    .visuals = {
        .brightness = 0.05f,
        .saturation = 1.0f,
        .contrast = 1.35f,
    },
};


Theme theme4Coder = {
    .backgroundColor = ColorFromHex(0xFF040404),
    .hoverableItemForegroundColor = ColorFromHex(0xCCfffc7f),
    .hoverableItemBackgroundColor = ColorFromHex(0xFF232330),
    .selectorBackground = ColorFromHex(0xFF040404),
    .searchBackgroundColor = ColorFromHex(0xff87ffaf),
    .selectableListBackground = ColorFromHex(0xFF1C1C1C),
    .searchWordColor = ColorFromHex(0xFF0C0C0C),
    .backgroundLineNumbers = ColorFromHex(0xFF040404),
    .lineNumberColor = ColorFromHex(0xFF4C4D4E),
    .lineNumberHighlitedColor = ColorFromHex(0xFF7a7c7d),
    .cursorLineHighlight = ColorFromHex(0xFF181824),
    .fileHeaderColor = ColorFromHex(0xCCffc110),
    .operatorColor   = ColorFromHex(0xCCffc110),

    .datatypeColor   = ColorFromHex(0xCCffc110),
    .commentColor    = ColorFromHex(0xCCa3a3a3),
    .commentTodoColor = ColorFromHex(0xFFA8181F),
    .commentNoteColor = ColorFromHex(0XFF18A81F),
    .commentImportantColor = ColorFromHex(0xFFA8A81F),
    .stringColor     = ColorFromHex(0xCC8bb92d),
    .numberColor     = ColorFromHex(0xCC8bb92d),
    .reservedColor   = ColorFromHex(0xCCd0ad81),
    .functionColor   = ColorFromHex(0xCC8bb92e),
    .includeColor    = ColorFromHex(0xCC8bb92e),
    .mathColor       = ColorFromHex(0xCCdeb82e),
    .tokensColor     = ColorFromHex(0xDDd0ad81),
    .tokensOverCursorColor = ColorFromHex(0xFF0C0C0C),
    .preprocessorColor = ColorFromHex(0xCCfff0ba),
    .preprocessorDefineColor = ColorFromHex(0xCCfff0ba),

    //.preprocessorDefineColor = ColorFromHex(0xFFDD1A01),
    .borderColor = ColorFromHex(0xFFFF7F50),
    .braces = ColorFromHex(0xFF00FFFF),
    .cursorColor = ColorFromHex(0xFF40FF40),
    //.cursorColor = ColorFromHex(0xFFFF4040),
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
        ColorFromHex(0xFF080202),
        ColorFromHex(0xFF020802),
        ColorFromHex(0xFF020208),
        ColorFromHex(0xFF080802),
    },

    //.userDatatypeColor = ColorFromHex(0xCC8EE7BF),
    .userDatatypeColor = ColorFromHex(0xCCffc110),//0xCCffdf10), //223, 16
    .userDatatypeEnum = ColorFromHex(0xCCffffe1),
    //.userDatatypeColor = ColorFromHex(0xCC00CCCC),
    .scopeLineColor = ColorFromHex(0xAAAAAAAA),
    .scrollbarColor = ColorFromHex(0xff353f25),
    .querybarTypeLineColor = ColorFromHex(0xFF330d0d),
    .selectorLoadedColor = ColorFromHex(0xFFCB9401),
    .userDefineColor = ColorFromHex(0xCCff628a),
    //.userDefineColor = ColorFromHex(0xCC28d5c7),
    .backTextCount = 4,
    .lineBorderWidth = 0,
    .alphaDimm = 0,
    .dynamicCursor = false,
    .borderWidth = 3,
    .pasteColor = ColorFromHex(0xffffddee),
    .dbgArrowColor = ColorFromHex(0xffffffff),
    .dbgLinehighlightColor = ColorFromHex(0xFF232340),
    .mouseSelectionColor = ColorFromHex(0xFF232333),
    .isLight = false,
    .renderLineNumberDiff = false,
    .visuals = {
        .brightness = 0.05f,
        .saturation = 1.0f,
        .contrast = 1.35f,
    },
};

Theme themeNoah = {
    .backgroundColor = ColorFromHex(0xff21222f),
    .hoverableItemForegroundColor = ColorFromHex(0xCCfffc7f),
    .hoverableItemBackgroundColor = ColorFromHex(0xFF232330),
    .selectorBackground = ColorFromHex(0xff21222f),
    .searchBackgroundColor = ColorFromHex(0xff87ffaf),
    .selectableListBackground = ColorFromHex(0xff21222f),
    .searchWordColor = ColorFromHex(0xff380c2a),
    .backgroundLineNumbers = ColorFromHex(0xff21222f),
    .lineNumberColor = ColorFromHex(0xff3e455f),
    .lineNumberHighlitedColor = ColorFromHex(0xff565f83),
    .cursorLineHighlight = ColorFromHex(0xff252838),
    .fileHeaderColor = ColorFromHex(0xff00cdd0),

    .operatorColor   = ColorFromHex(0xffffcf7a),
    .datatypeColor   = ColorFromHex(0xff00cdd0),
    .commentColor    = ColorFromHex(0xff565f83),
    .commentTodoColor = ColorFromHex(0xffffcf7a),
    .commentNoteColor = ColorFromHex(0xffffcf7a),
    .commentImportantColor = ColorFromHex(0xffffcf7a),
    .stringColor     = ColorFromHex(0xffc2ed94),
    .numberColor     = ColorFromHex(0xffffcf7a),
    .reservedColor   = ColorFromHex(0xff00cdd0),
    .functionColor   = ColorFromHex(0xff00f5c7),
    .includeColor    = ColorFromHex(0xffffcf7a),
    .mathColor       = ColorFromHex(0xff00cdd0),
    .tokensColor     = ColorFromHex(0xffb3d2db),
    .tokensOverCursorColor = ColorFromHex(0xff21222f),
    .preprocessorColor = ColorFromHex(0xCCfff0ba),
    .preprocessorDefineColor = ColorFromHex(0xffffcf7a),

    .borderColor = ColorFromHex(0xFFFF7F50),
    .braces = ColorFromHex(0xFF00FFFF),
    .cursorColor = ColorFromHex(0xff00f5c7),
    .querybarCursorColor = ColorFromHex(0xff00f5c7),
    .ghostCursorColor = ColorFromHex(0xff00f5c7),
    .parenthesis = {
        ColorFromHex(0xFFFF0000),
        ColorFromHex(0xFF00FF00),
        ColorFromHex(0xFF0000FF),
        ColorFromHex(0xFFAAAA00),
    },
    .backTextColors = {
        ColorFromHex(0xff21222f),
    },

    .userDatatypeColor = ColorFromHex(0xff00cdd0),
    .userDatatypeEnum = ColorFromHex(0xCCffffe1),
    .scopeLineColor = ColorFromHex(0xAAAAAAAA),
    .scrollbarColor = ColorFromHex(0xff303144),
    .querybarTypeLineColor = ColorFromHex(0xFF330d0d),
    .selectorLoadedColor = ColorFromHex(0xFFCB9401),
    .userDefineColor = ColorFromHex(0xffffcf7a),
    .backTextCount = 1,
    .lineBorderWidth = 0,
    .alphaDimm = 0,
    .dynamicCursor = false,
    .borderWidth = 3,
    .pasteColor = ColorFromHex(0xffffddee),
    .dbgArrowColor = ColorFromHex(0xffffffff),
    .dbgLinehighlightColor = ColorFromHex(0xff252848),
    .mouseSelectionColor = ColorFromHex(0xff252838),
    .isLight = false,
    .renderLineNumberDiff = false,
    .visuals = {
        .brightness = 0.05f,
        .saturation = 1.0f,
        .contrast = 1.0f,
    },
};

Theme themeGreenish = {
    .backgroundColor = ColorFromHex(0xff0d0d0d),
    .hoverableItemForegroundColor = ColorFromHex(0xffe6f5e6),
    .hoverableItemBackgroundColor = ColorFromHex(0xff002a00),
    .selectorBackground = ColorFromHex(0xff0d0d0d),

    .searchBackgroundColor = ColorFromHex(0xff1a5e1a),
    .selectableListBackground = ColorFromHex(0xff0a1a0a),
    .searchWordColor = ColorFromHex(0xffa3dca3),
    .backgroundLineNumbers = ColorFromHex(0xff0d0d0d),
    .lineNumberColor = ColorFromHex(0xff6cd96c),
    .lineNumberHighlitedColor = ColorFromHex(0xffa3ffa3),
    .cursorLineHighlight = ColorFromHex(0xff111911),
    .fileHeaderColor = ColorFromHex(0xff2ab32a),

    .operatorColor   = ColorFromHex(0xff6dd36d),
    .datatypeColor   = ColorFromHex(0xff3ca23c),
    .commentColor    = ColorFromHex(0xff3d6d3d),
    .commentTodoColor = ColorFromHex(0xff44a944),
    .commentNoteColor = ColorFromHex(0xff66bf66),
    .commentImportantColor = ColorFromHex(0xffcce6cc),
    .stringColor     = ColorFromHex(0xff508f50),
    .numberColor     = ColorFromHex(0xff369436),
    .reservedColor   = ColorFromHex(0xff287a28),
    .functionColor   = ColorFromHex(0xff3ab53a),
    .includeColor    = ColorFromHex(0xff388738),

    .mathColor       = ColorFromHex(0xff69d369),

    .tokensColor     = ColorFromHex(0xffa3dca3),
    .tokensOverCursorColor = ColorFromHex(0xff0a2a0a),

    .preprocessorColor = ColorFromHex(0xff46b846),
    .preprocessorDefineColor = ColorFromHex(0xff87d687),

    .borderColor = ColorFromHex(0xff133f13),
    .braces = ColorFromHex(0xff33aa33),
    .cursorColor = ColorFromHex(0xff44a944),
    .querybarCursorColor = ColorFromHex(0xffb3d9b3),
    .ghostCursorColor = ColorFromHex(0xff46a146),
    .parenthesis = {
        ColorFromHex(0xff2a8c2a),
        ColorFromHex(0xff287328),
        ColorFromHex(0xff1f6a1f),
        ColorFromHex(0xff289928),
    },
    .backTextColors = {
        ColorFromHex(0xff0d0d0d),
    },

    .userDatatypeColor = ColorFromHex(0xff34ae34),
    .userDatatypeEnum = ColorFromHex(0xff6cc56c),
    .scopeLineColor = ColorFromHex(0xAAAAAAAA),
    .scrollbarColor = ColorFromHex(0xff191919),
    .querybarTypeLineColor = ColorFromHex(0xff0a2a0a),
    .selectorLoadedColor = ColorFromHex(0xff38a938),
    .userDefineColor = ColorFromHex(0xff46b846),
    .backTextCount = 1,
    .lineBorderWidth = 0, // No border line for cursor line
    .alphaDimm = 0,
    .dynamicCursor = false,
    .borderWidth = 2,
    .pasteColor = ColorFromHex(0xffbbe0bb),
    .dbgArrowColor = ColorFromHex(0xff69cc69),
    .dbgLinehighlightColor = ColorFromHex(0xff191919),
    .mouseSelectionColor = ColorFromHex(0x112c4d2c),
    .isLight = false,
    .renderLineNumberDiff = false,
    .visuals = {
        .brightness = 0.1f,
        .saturation = 0.8f,
        .contrast = 1.2f,
    },
};

Theme themeHalloween = {
    .backgroundColor = ColorFromHex(0xff0d0d0d),
    .hoverableItemForegroundColor = ColorFromHex(0xfff5e6e6),
    .hoverableItemBackgroundColor = ColorFromHex(0xff2a0000),
    .selectorBackground = ColorFromHex(0xff0d0d0d),

    .searchBackgroundColor = ColorFromHex(0xff5e1a1a),
    .selectableListBackground = ColorFromHex(0xff1a0a0a),
    .searchWordColor = ColorFromHex(0xffdca3a3),
    .backgroundLineNumbers = ColorFromHex(0xff0d0d0d),
    .lineNumberColor = ColorFromHex(0xffd96c6c),
    .lineNumberHighlitedColor = ColorFromHex(0xffffa3a3),
    .cursorLineHighlight = ColorFromHex(0xff191111),
    .fileHeaderColor = ColorFromHex(0xffb32a2a),

    .operatorColor   = ColorFromHex(0xffd36d6d),
    .datatypeColor   = ColorFromHex(0xffa23c3c),
    .commentColor    = ColorFromHex(0xff6d3d3d),
    .commentTodoColor = ColorFromHex(0xffa94444),
    .commentNoteColor = ColorFromHex(0xffbf6666),
    .commentImportantColor = ColorFromHex(0xffe6cccc),
    .stringColor     = ColorFromHex(0xff8f5050),
    .numberColor     = ColorFromHex(0xff943636),
    .reservedColor   = ColorFromHex(0xff7a2828),
    .functionColor   = ColorFromHex(0xffb53a3a),
    .includeColor    = ColorFromHex(0xff873838),

    .mathColor       = ColorFromHex(0xffd36969),

    .tokensColor     = ColorFromHex(0xffdca3a3),
    .tokensOverCursorColor = ColorFromHex(0xff2a0a0a),

    .preprocessorColor = ColorFromHex(0xffb84646),
    .preprocessorDefineColor = ColorFromHex(0xffd68787),

    .borderColor = ColorFromHex(0xff3f1313),
    .braces = ColorFromHex(0xffaa3333),
    .cursorColor = ColorFromHex(0xffa94444),
    .querybarCursorColor = ColorFromHex(0xffd9b3b3),
    .ghostCursorColor = ColorFromHex(0xffa14646),
    .parenthesis = {
        ColorFromHex(0xff8c2a2a),
        ColorFromHex(0xff732828),
        ColorFromHex(0xff6a1f1f),
        ColorFromHex(0xff992828),
    },
    .backTextColors = {
        ColorFromHex(0xff0d0d0d),
    },

    .userDatatypeColor = ColorFromHex(0xffae3434),
    .userDatatypeEnum = ColorFromHex(0xffc56c6c),
    .scopeLineColor = ColorFromHex(0xAAAAAAAA),
    .scrollbarColor = ColorFromHex(0xff191919),
    .querybarTypeLineColor = ColorFromHex(0xff2a0a0a),
    .selectorLoadedColor = ColorFromHex(0xffa93838),
    .userDefineColor = ColorFromHex(0xffb84646),
    .backTextCount = 1,
    .lineBorderWidth = 0, // No border line for cursor line
    .alphaDimm = 0,
    .dynamicCursor = false,
    .borderWidth = 2,
    .pasteColor = ColorFromHex(0xffe0bbbb),
    .dbgArrowColor = ColorFromHex(0xffcc6969),
    .dbgLinehighlightColor = ColorFromHex(0xff191919),
    .mouseSelectionColor = ColorFromHex(0x114d2c2c),
    .isLight = false,
    .renderLineNumberDiff = false,
    .visuals = {
        .brightness = 0.1f,
        .saturation = 0.8f,
        .contrast = 1.2f,
    },
};

// swap this to make the default theme, i.e.: theme enabled when opening
// otherwise use swap theme for changing between these

Theme *defaultTheme = &themeEverDark;
//Theme *defaultTheme = &themeToxi;
//Theme *defaultTheme = &themeGrayVim;
//Theme *defaultTheme = &themeHalloween;
//Theme *defaultTheme = &themeGreenish;
//Theme *defaultTheme = &theme4coderAlt;
//Theme *defaultTheme = &themeDarkVim;
//Theme *defaultTheme = &themeCatppuccin;
//Theme *defaultTheme = &themeRadical;
//Theme *defaultTheme = &theme4Coder;
//Theme *defaultTheme = &themeDracula;
//Theme *defaultTheme = &themeYavid;
//Theme *defaultTheme = &themeTerminal;
//Theme *defaultTheme = &themeGruvboxLight;
//Theme *defaultTheme = &themeNoah;

static
bool defaultLineNumberMode = defaultTheme->renderLineNumberDiff;

std::vector<ThemeDescription> themesDesc = {
    { .name = "Radical", .theme = &themeRadical, },
    { .name = "4coder", .theme = &theme4Coder, },
    { .name = "4coderAlt", .theme = &theme4coderAlt, },
    { .name = "Dracula", .theme = &themeDracula, },
    { .name = "Gruvbox Light", .theme = &themeGruvboxLight, },
    { .name = "Yavid", .theme = &themeYavid, },
    { .name = "Terminal", .theme = &themeTerminal, },
    { .name = "Noah", .theme = &themeNoah, },
    { .name = "Catppuccin", .theme = &themeCatppuccin, },
    { .name = "DarkVim", .theme = &themeDarkVim, },
    { .name = "GrayVim", .theme = &themeGrayVim, },
    { .name = "GreenVilage", .theme = &themeGreenish, },
    { .name = "Halloween", .theme = &themeHalloween, },
    { .name = "Toxi", .theme = &themeToxi, },
    { .name = "EverDark", .theme = &themeEverDark, }
};

static int globalActive = 0;
void SetAlpha(int active){
    globalActive = active;
}

void SwapLineNumberRenderinType(){
    defaultLineNumberMode = !defaultLineNumberMode;
}

inline vec4i ApplyAlpha(vec4i color, Theme *theme){
    if(theme->alphaDimm){
        Float w = (Float)color.w;
        if(globalActive) w *= kAlphaReduceInactive;
        else w *= kAlphaReduceDefault;
        color.w = (int)w;
    }

    return color;
}

void GetThemeList(std::vector<ThemeDescription> **themes){
    if(themes){
        *themes = &themesDesc;
    }
}

void SwapDefaultTheme(char *name, uint len){
    for(uint i = 0; i < themesDesc.size(); i++){
        ThemeDescription *desc = &themesDesc[i];
        uint slen = (uint)strlen(desc->name);
        if(slen == len){
            if(StringEqual(name, (char *)desc->name, len)){
                defaultTheme = desc->theme;
                break;
            }
        }
    }
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
        COLOR_RET(TOKEN_ID_COMMENT_IMPORTANT, commentImportantColor);
        COLOR_RET(TOKEN_ID_COMMENT_TODO, commentTodoColor);
        COLOR_RET(TOKEN_ID_COMMENT_NOTE, commentNoteColor);
        COLOR_RET(TOKEN_ID_STRING, stringColor);
        COLOR_RET(TOKEN_ID_NUMBER, numberColor);
        COLOR_RET(TOKEN_ID_RESERVED, reservedColor);
        COLOR_RET(TOKEN_ID_FUNCTION, functionColor);
        // TODO: Customize the function declaration
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
        COLOR_RET(TOKEN_ID_DATATYPE_NAMESPACE_DEF, operatorColor);
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
        COLOR_RET(TOKEN_ID_DATATYPE_USER_NAMESPACE, userDatatypeColor);
        COLOR_RET(TOKEN_ID_DATATYPE_USER_ENUM_VALUE, userDatatypeEnum);
        COLOR_RET(TOKEN_ID_DATATYPE_USER_ENUM, userDatatypeColor);


        COLOR_RET(TOKEN_ID_PREPROCESSOR_DEFINITION, userDefineColor);
        default:{
            printf("Token id is %s (%d)\n", Symbol_GetIdString(id), (int)id);
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
        COLOR_RET(UIFileHeader, fileHeaderColor);
        COLOR_RET(UILineNumberHighlighted, lineNumberHighlitedColor);
        COLOR_RET(UILineNumbers, lineNumberColor);
        COLOR_RET(UIBorder, borderColor);
        COLOR_RET(UICursor, cursorColor);
        COLOR_RET(UIQueryBarCursor, querybarCursorColor);
        COLOR_RET(UIGhostCursor, ghostCursorColor);
        COLOR_RET(UISelectableListBackground, selectableListBackground);
        COLOR_RET(UIScopeLine, scopeLineColor);
        COLOR_RET(UICharOverCursor, tokensOverCursorColor);
        COLOR_RET(UIScrollBarColor, scrollbarColor);
        COLOR_RET(UIQueryBarTypeColor, querybarTypeLineColor);
        COLOR_RET(UISelectorLoadedColor, selectorLoadedColor);
        COLOR_RET(UIHoverableListItem, hoverableItemForegroundColor);
        COLOR_RET(UIHoverableListItemBackground, hoverableItemBackgroundColor);
        COLOR_RET(UIPasteColor, pasteColor);
        COLOR_RET(UIDbgArrowColor, dbgArrowColor);
        COLOR_RET(UIDbgLineHighlightColor, dbgLinehighlightColor);
        COLOR_RET(UISelectionColor, mouseSelectionColor);
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

int GetSelectorBorderWidth(Theme *theme){
    return theme->borderWidth;
}

bool CurrentThemeIsLight(){
    return defaultTheme->isLight;
}

bool CurrentThemeLineNumberByDiff(){
    return defaultLineNumberMode;
}

void CurrentThemeSetDimm(int dim){
    defaultTheme->alphaDimm = dim != 0 ? 1 : 0;
}

void CurrentThemeSetBrightness(Float value){
    defaultTheme->visuals.brightness = Max(value, 0.01f);
}

void CurrentThemeSetSaturation(Float value){
    defaultTheme->visuals.saturation = Max(value, 0.01f);
}

void CurrentThemeSetContrast(Float value){
    defaultTheme->visuals.contrast = Max(value, 0.01f);
}

void CurrentThemeGetVisuals(ThemeVisuals &out){
    out.brightness = defaultTheme->visuals.brightness;
    out.contrast = defaultTheme->visuals.contrast;
    out.saturation = defaultTheme->visuals.saturation;
}
