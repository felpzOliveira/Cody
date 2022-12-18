//
// Copyright (c) 2009-2013 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

//
// NOTE: Modified in January 13/2021 by Felipe.
// Added 'fonsStashMultiTextColor' and 'fonsStashFlush' to improve performance
// by reducing render calls and allow multi-color texts in one render pass.
// Update 0: Added 'fonsGetGlyphRectangle' to expose rectangles coordinates.
// Update 1: Decoupled state and user params for shared context rendering
//

#ifndef FONS_H
#define FONS_H

#ifdef __cplusplus
extern "C" {
#endif
#define FONS_DEFAULT_X_SPACING 0
    // To make the implementation private to the file that generates the implementation
#ifdef FONS_STATIC
#define FONS_DEF static
#else
#define FONS_DEF extern
#endif

#define FONS_INVALID -1

    enum FONSflags {
        FONS_ZERO_TOPLEFT = 1,
        FONS_ZERO_BOTTOMLEFT = 2,
    };

    enum FONSalign {
        // Horizontal align
        FONS_ALIGN_LEFT 	= 1<<0,	// Default
        FONS_ALIGN_CENTER 	= 1<<1,
        FONS_ALIGN_RIGHT 	= 1<<2,
        // Vertical align
        FONS_ALIGN_TOP 		= 1<<3,
        FONS_ALIGN_MIDDLE	= 1<<4,
        FONS_ALIGN_BOTTOM	= 1<<5,
        FONS_ALIGN_BASELINE	= 1<<6, // Default
    };

    enum FONSerrorCode {
        // Font atlas is full.
        FONS_ATLAS_FULL = 1,
        // Scratch memory used to render glyphs is full, requested size reported in 'val', you may need to bump up FONS_SCRATCH_BUF_SIZE.
        FONS_SCRATCH_FULL = 2,
        // Calls to fonsPushState has created too large stack, if you need deep state stack bump up FONS_MAX_STATES.
        FONS_STATES_OVERFLOW = 3,
        // Trying to pop too many states fonsPopState().
        FONS_STATES_UNDERFLOW = 4,
    };

    struct FONSparams {
        int width, height;
        unsigned char flags;
        void* userPtr;
        int (*renderCreate)(void* uptr, int width, int height);
        int (*renderResize)(void* uptr, int width, int height);
        void (*renderUpdate)(void* uptr, int* rect, const unsigned char* data);
        void (*renderDraw)(void* uptr, const float* verts, const float* tcoords, const unsigned int* colors, int nverts);
        void (*renderDelete)(void* uptr);
    };
    typedef struct FONSparams FONSparams;

    struct FONSquad
    {
        float x0,y0,s0,t0;
        float x1,y1,s1,t1;
    };
    typedef struct FONSquad FONSquad;

    struct FONStextIter {
        float x, y, nextx, nexty, scale, spacing;
        unsigned int codepoint;
        short isize, iblur;
        struct FONSfont* font;
        int prevGlyphIndex;
        const char* str;
        const char* next;
        const char* end;
        unsigned int utf8state;
    };
    typedef struct FONStextIter FONStextIter;

    // See also: stb_truetype documentation for stbtt_GetGlyphSDF. These parameters are copied from there
    struct FONSsdfSettings
    {
        unsigned char sdfEnabled;
        unsigned char onedgeValue; // value 0-255 to test the SDF against to reconstruct the character (i.e. the isocontour of the character)

        int padding;               // extra "pixels" around the character which are filled with the distance to the character (not 0),
        // which allows effects like bit outlines

        float pixelDistScale;      // what value the SDF should increase by when moving one SDF "pixel" away from the edge (on the 0..255 scale)
        // if positive, > onedge_value is inside; if negative, < onedge_value is inside
    };
    typedef struct FONSsdfSettings FONSsdfSettings;


    typedef struct FONScontext FONScontext;
    typedef struct FONScontextImpl FONScontextImpl;

    // Contructor and destructor.
    FONS_DEF FONScontext* fonsCreateInternal(FONSparams* params);
    FONS_DEF FONScontext* fonsCreateInternalFrom(FONSparams* params, FONScontext* other);
    FONS_DEF void fonsDeleteInternal(FONScontext* s);

    FONS_DEF void fonsSetErrorCallback(FONScontext* s, void (*callback)(void* uptr, int error, int val), void* uptr);
    // Returns current atlas size.
    FONS_DEF void fonsGetAtlasSize(FONScontext* s, int* width, int* height);
    // Expands the atlas size. 
    FONS_DEF int fonsExpandAtlas(FONScontext* s, int width, int height);
    // Resets the whole stash.
    FONS_DEF int fonsResetAtlas(FONScontext* stash, int width, int height);

    // Add fonts
    FONS_DEF int fonsAddFont(FONScontext* s, const char* name, const char* path);
    FONS_DEF int fonsAddFontMem(FONScontext* s, const char* name, unsigned char* data, int ndata, int freeData);

#ifndef FONS_USE_FREETYPE
    FONS_DEF int fonsAddFontSdf(FONScontext* s, const char* name, const char* path, FONSsdfSettings sdfSettings);
    FONS_DEF int fonsAddFontSdfMem(FONScontext* s, const char* name, unsigned char* data, int ndata, int freeData, FONSsdfSettings sdfSettings);
#endif

    FONS_DEF int fonsGetFontByName(FONScontext* s, const char* name);
    FONS_DEF int fonsAddFallbackFont(FONScontext* stash, int base, int fallback);

    // State handling
    FONS_DEF void fonsPushState(FONScontext* s);
    FONS_DEF void fonsPopState(FONScontext* s);
    FONS_DEF void fonsClearState(FONScontext* s);

    // State setting
    FONS_DEF void fonsSetSize(FONScontext* s, float size);
    FONS_DEF void fonsSetColor(FONScontext* s, unsigned int color);
    FONS_DEF void fonsSetSpacing(FONScontext* s, float spacing);
    FONS_DEF void fonsSetBlur(FONScontext* s, float blur);
    FONS_DEF void fonsSetAlign(FONScontext* s, int align);
    FONS_DEF void fonsSetFont(FONScontext* s, int font);

    // Draw text
    FONS_DEF float fonsDrawText(FONScontext* s, float x, float y, const char* string, const char* end);

    // Check if codepoint is available at the current font
    FONS_DEF int fonsGetGlyphIndex(FONScontext* stash, int codepoint);

    // Stack a piece of text in a color into the stash buffer. Renders only when needed.
    FONS_DEF float fonsStashMultiTextColor(FONScontext* stash,
                                           float x, float y, unsigned int color,
                                           const char* str, const char* end,
                                           int *prevGlyph);

    // Computes the movement in 'x' required to render the first 'n' characters of
    // the string 'e'
    FONS_DEF float fonsComputeStringAdvance(FONScontext* stash, const char *e, int n,
                                            int *prevGlyph);

    // Computes the amount of characters that need to be rendered to get to position 'x1'
    FONS_DEF int fonsComputeStringOffsetCount(FONScontext* stash, const char *e, float x1);

    // Force a render call on the current stash
    FONS_DEF void fonsStashFlush(FONScontext *stash);

    // Measure text
    FONS_DEF float fonsTextBounds(FONScontext* s, float x, float y, const char* string, const char* end, float* bounds);
    FONS_DEF void fonsLineBounds(FONScontext* s, float y, float* miny, float* maxy);
    FONS_DEF void fonsVertMetrics(FONScontext* s, float* ascender, float* descender, float* lineh);

    // Text iterator
    FONS_DEF int fonsTextIterInit(FONScontext* stash, FONStextIter* iter, float x, float y, const char* str, const char* end);
    FONS_DEF int fonsTextIterNext(FONScontext* stash, FONStextIter* iter, struct FONSquad* quad);

    // Pull texture changes
    FONS_DEF const unsigned char* fonsGetTextureData(FONScontext* stash, int* width, int* height);
    FONS_DEF int fonsValidateTexture(FONScontext* s, int* dirty);

    // Draws the stash texture for debugging
    FONS_DEF void fonsDrawDebug(FONScontext* s, float x, float y);

#ifdef __cplusplus
}
#endif

#endif // FONS_H


#ifdef FONTSTASH_IMPLEMENTATION

#define FONS_NOTUSED(v)  (void)sizeof(v)

#ifdef FONS_USE_FREETYPE

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H
#include <math.h>

struct FONSttFontImpl {
	FT_Face font;
};
typedef struct FONSttFontImpl FONSttFontImpl;

static FT_Library ftLibrary;

static int fons__tt_init()
{
	FT_Error ftError;
	FONS_NOTUSED(context);
	ftError = FT_Init_FreeType(&ftLibrary);
	return ftError == 0;
}

static int fons__tt_loadFont(FONScontext *context, FONSttFontImpl *font, unsigned char *data, int dataSize)
{
	FT_Error ftError;
	FONS_NOTUSED(context);

	//font->font.userdata = stash;
	ftError = FT_New_Memory_Face(ftLibrary, (const FT_Byte*)data, dataSize, 0, &font->font);
	return ftError == 0;
}

static void fons__tt_getFontVMetrics(FONSttFontImpl *font, int *ascent, int *descent, int *lineGap)
{
	*ascent = font->font->ascender;
	*descent = font->font->descender;
	*lineGap = font->font->height - (*ascent - *descent);
}

static float fons__tt_getPixelHeightScale(FONSttFontImpl *font, float size)
{
	return size / (font->font->ascender - font->font->descender);
}

static int fons__tt_getGlyphIndex(FONSttFontImpl *font, int codepoint)
{
	return FT_Get_Char_Index(font->font, codepoint);
}

static int fons__tt_buildGlyphBitmap(FONSttFontImpl *font, int glyph, float size, float scale,
                                     int *advance, int *lsb, int *x0, int *y0, int *x1, int *y1, const FONSsdfSettings* sdfSettings)
{
	FT_Error ftError;
	FT_GlyphSlot ftGlyph;
	FT_Fixed advFixed;
	FONS_NOTUSED(scale);
	FONS_NOTUSED(sdfSettings);

	ftError = FT_Set_Pixel_Sizes(font->font, 0, (FT_UInt)(size * (float)font->font->units_per_EM / (float)(font->font->ascender - font->font->descender)));
	if (ftError) return 0;
	ftError = FT_Load_Glyph(font->font, glyph, FT_LOAD_RENDER);
	if (ftError) return 0;
	ftError = FT_Get_Advance(font->font, glyph, FT_LOAD_NO_SCALE, &advFixed);
	if (ftError) return 0;
	ftGlyph = font->font->glyph;
	*advance = (int)advFixed;
	*lsb = (int)ftGlyph->metrics.horiBearingX;
	*x0 = ftGlyph->bitmap_left;
	*x1 = *x0 + ftGlyph->bitmap.width;
	*y0 = -ftGlyph->bitmap_top;
	*y1 = *y0 + ftGlyph->bitmap.rows;
	return 1;
}

static void fons__tt_renderGlyphBitmap(FONSttFontImpl *font, unsigned char *output, int outWidth, int outHeight, int outStride,
                                       float scaleX, float scaleY, int glyph, const FONSsdfSettings* sdfSettings)
{
	FT_GlyphSlot ftGlyph = font->font->glyph;
	int ftGlyphOffset = 0;
	int x, y;
	FONS_NOTUSED(outWidth);
	FONS_NOTUSED(outHeight);
	FONS_NOTUSED(scaleX);
	FONS_NOTUSED(scaleY);
	FONS_NOTUSED(glyph);	// glyph has already been loaded by fons__tt_buildGlyphBitmap
	FONS_NOTUSED(sdfSettings);

	for ( y = 0; y < ftGlyph->bitmap.rows; y++ ) {
		for ( x = 0; x < ftGlyph->bitmap.width; x++ ) {
			output[(y * outStride) + x] = ftGlyph->bitmap.buffer[ftGlyphOffset++];
		}
	}
}

static int fons__tt_getGlyphKernAdvance(FONSttFontImpl *font, int glyph1, int glyph2)
{
	FT_Vector ftKerning;
	FT_Get_Kerning(font->font, glyph1, glyph2, FT_KERNING_DEFAULT, &ftKerning);
	return (int)((ftKerning.x + 32) >> 6);  // Round up and convert to integer
}

#else

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
static void* fons__tmpalloc(size_t size, void* up);
static void fons__tmpfree(void* ptr, void* up);
#define STBTT_malloc(x,u)    fons__tmpalloc(x,u)
#define STBTT_free(x,u)      fons__tmpfree(x,u)
#include "stb_truetype.h"

struct FONSttFontImpl {
	stbtt_fontinfo font;
};
typedef struct FONSttFontImpl FONSttFontImpl;

static int fons__tt_init(FONScontextImpl *context)
{
	FONS_NOTUSED(context);
	return 1;
}

static int fons__tt_loadFont(FONScontextImpl *context, FONSttFontImpl *font, unsigned char *data, int dataSize)
{
	int stbError;
	FONS_NOTUSED(dataSize);

	font->font.userdata = context;
	stbError = stbtt_InitFont(&font->font, data, 0);
	return stbError;
}

static void fons__tt_getFontVMetrics(FONSttFontImpl *font, int *ascent, int *descent, int *lineGap)
{
	stbtt_GetFontVMetrics(&font->font, ascent, descent, lineGap);
}

static float fons__tt_getPixelHeightScale(FONSttFontImpl *font, float size)
{
	return stbtt_ScaleForPixelHeight(&font->font, size);
}

static int fons__tt_getGlyphIndex(FONSttFontImpl *font, int codepoint)
{
	return stbtt_FindGlyphIndex(&font->font, codepoint);
}

static int fons__tt_buildGlyphBitmap(FONSttFontImpl *font, int glyph, float size, float scale,
                                     int *advance, int *lsb, int *x0, int *y0, int *x1, int *y1, const FONSsdfSettings* sdfSettings)
{
	FONS_NOTUSED(size);
	stbtt_GetGlyphHMetrics(&font->font, glyph, advance, lsb);
	stbtt_GetGlyphBitmapBox(&font->font, glyph, scale, scale, x0, y0, x1, y1);

	if (sdfSettings->sdfEnabled)
	{
		*x0 -= sdfSettings->padding;
		*y0 -= sdfSettings->padding;
		*x1 += sdfSettings->padding;
		*y1 += sdfSettings->padding;
	}

	return 1;
}

static void fons__tt_renderGlyphBitmap(FONSttFontImpl *font, unsigned char *output, int outWidth, int outHeight, int outStride,
                                       float scaleX, float scaleY, int glyph, const FONSsdfSettings* sdfSettings)
{
	if (!sdfSettings->sdfEnabled)
	{
		stbtt_MakeGlyphBitmap(&font->font, output, outWidth, outHeight, outStride, scaleX, scaleY, glyph);
	}
	else
	{
		int w = 0, h = 0, xoff = 0, yoff = 0;
		unsigned char* sdfData = stbtt_GetGlyphSDF(&font->font, scaleX, glyph, sdfSettings->padding, sdfSettings->onedgeValue, sdfSettings->pixelDistScale, &w, &h, &xoff, &yoff);

		for (int y = 0; y < h; y++)
		{
			unsigned char* outRow = output + (outStride*y);
			unsigned char* inRow = sdfData + (w*y);

			memcpy(outRow, inRow, w);
		}

		if (sdfData)
            stbtt_FreeSDF(sdfData, NULL);
	}
}

static int fons__tt_getGlyphKernAdvance(FONSttFontImpl *font, int glyph1, int glyph2)
{
	return stbtt_GetGlyphKernAdvance(&font->font, glyph1, glyph2);
}

#endif

#ifndef FONS_SCRATCH_BUF_SIZE
#	define FONS_SCRATCH_BUF_SIZE 64000
#endif
#ifndef FONS_HASH_LUT_SIZE
#	define FONS_HASH_LUT_SIZE 256
#endif
#ifndef FONS_INIT_FONTS
#	define FONS_INIT_FONTS 4
#endif
#ifndef FONS_INIT_GLYPHS
#	define FONS_INIT_GLYPHS 256
#endif
#ifndef FONS_INIT_ATLAS_NODES
#	define FONS_INIT_ATLAS_NODES 256
#endif
#ifndef FONS_VERTEX_COUNT
#	define FONS_VERTEX_COUNT 1024
#endif
#ifndef FONS_MAX_STATES
#	define FONS_MAX_STATES 20
#endif
#ifndef FONS_MAX_FALLBACKS
#	define FONS_MAX_FALLBACKS 20
#endif

static unsigned int fons__hashint(unsigned int a)
{
	a += ~(a<<15);
	a ^=  (a>>10);
	a +=  (a<<3);
	a ^=  (a>>6);
	a += ~(a<<11);
	a ^=  (a>>16);
	return a;
}

static int fons__mini(int a, int b)
{
	return a < b ? a : b;
}

static int fons__maxi(int a, int b)
{
	return a > b ? a : b;
}

struct FONSglyph
{
	unsigned int codepoint;
	int index;
	int next;
	short size, blur;
	short x0,y0,x1,y1;
	short xadv,xoff,yoff;
};
typedef struct FONSglyph FONSglyph;

struct FONSfont
{
	FONSttFontImpl font;
	char name[64];
	unsigned char* data;
	int dataSize;
	unsigned char freeData;
	float ascender;
	float descender;
	float lineh;
	FONSglyph* glyphs;
	int cglyphs;
	int nglyphs;
	int lut[FONS_HASH_LUT_SIZE];
	int fallbacks[FONS_MAX_FALLBACKS];
	int nfallbacks;
	FONSsdfSettings sdfSettings;
};
typedef struct FONSfont FONSfont;

struct FONSstate
{
	int font;
	int align;
	float size;
	unsigned int color;
	float blur;
	float spacing;
};
typedef struct FONSstate FONSstate;

struct FONSatlasNode {
	short x, y, width;
};
typedef struct FONSatlasNode FONSatlasNode;

struct FONSatlas
{
	int width, height;
	FONSatlasNode* nodes;
	int nnodes;
	int cnodes;
};
typedef struct FONSatlas FONSatlas;

struct FONScontextImpl{
    float itw,ith;
	unsigned char* texData;
	int dirtyRect[4];
	FONSfont** fonts;
	FONSatlas* atlas;
	int cfonts;
	int nfonts;
	float verts[FONS_VERTEX_COUNT*2];
	float tcoords[FONS_VERTEX_COUNT*2];
	unsigned int colors[FONS_VERTEX_COUNT];
	int nverts;
	unsigned char* scratch;
	int nscratch;
	FONSstate states[FONS_MAX_STATES];
	int nstates;
	void (*handleError)(void* uptr, int error, int val);
	void* errorUptr;
};

struct FONScontext{
	FONSparams params;
    FONScontextImpl *ctxImpl;
};


#ifdef STB_TRUETYPE_IMPLEMENTATION

static void* fons__tmpalloc(size_t size, void* up)
{
	unsigned char* ptr;
	FONScontextImpl* stash = (FONScontextImpl*)up;

	// 16-byte align the returned pointer
	size = (size + 0xf) & ~0xf;

	if (stash->nscratch+(int)size > FONS_SCRATCH_BUF_SIZE) {
		if (stash->handleError)
			stash->handleError(stash->errorUptr, FONS_SCRATCH_FULL, stash->nscratch+(int)size);
		return NULL;
	}
	ptr = stash->scratch + stash->nscratch;
	stash->nscratch += (int)size;
	return ptr;
}

static void fons__tmpfree(void* ptr, void* up)
{
	(void)ptr;
	(void)up;
	// empty
}

#endif // STB_TRUETYPE_IMPLEMENTATION

// Copyright (c) 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

#define FONS_UTF8_ACCEPT 0
#define FONS_UTF8_REJECT 12

static unsigned int fons__decutf8(unsigned int* state, unsigned int* codep, unsigned int byte)
{
	static const unsigned char utf8d[] = {
		// The first part of the table maps bytes to character classes that
		// to reduce the size of the transition table and create bitmasks.
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
		7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
		8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
		10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,

		// The second part is a transition table that maps a combination
		// of a state of the automaton and a character class to a state.
		0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
		12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
		12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
		12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
		12,36,12,12,12,12,12,12,12,12,12,12,
	};

	unsigned int type = utf8d[byte];

	*codep = (*state != FONS_UTF8_ACCEPT) ?
		(byte & 0x3fu) | (*codep << 6) :
    (0xff >> type) & (byte);

	*state = utf8d[256 + *state + type];
	return *state;
}

// Atlas based on Skyline Bin Packer by Jukka Jylänki

static void fons__deleteAtlas(FONSatlas* atlas)
{
	if (atlas == NULL) return;
	if (atlas->nodes != NULL) free(atlas->nodes);
	free(atlas);
}

static FONSatlas* fons__allocAtlas(int w, int h, int nnodes)
{
	FONSatlas* atlas = NULL;

	// Allocate memory for the font stash.
	atlas = (FONSatlas*)malloc(sizeof(FONSatlas));
	if (atlas == NULL) goto error;
	memset(atlas, 0, sizeof(FONSatlas));

	atlas->width = w;
	atlas->height = h;

	// Allocate space for skyline nodes
	atlas->nodes = (FONSatlasNode*)malloc(sizeof(FONSatlasNode) * nnodes);
	if (atlas->nodes == NULL) goto error;
	memset(atlas->nodes, 0, sizeof(FONSatlasNode) * nnodes);
	atlas->nnodes = 0;
	atlas->cnodes = nnodes;

	// Init root node.
	atlas->nodes[0].x = 0;
	atlas->nodes[0].y = 0;
	atlas->nodes[0].width = (short)w;
	atlas->nnodes++;

	return atlas;

    error:
	if (atlas) fons__deleteAtlas(atlas);
	return NULL;
}

static int fons__atlasInsertNode(FONSatlas* atlas, int idx, int x, int y, int w)
{
	int i;
	// Insert node
	if (atlas->nnodes+1 > atlas->cnodes) {
		atlas->cnodes = atlas->cnodes == 0 ? 8 : atlas->cnodes * 2;
		atlas->nodes = (FONSatlasNode*)realloc(atlas->nodes, sizeof(FONSatlasNode) * atlas->cnodes);
		if (atlas->nodes == NULL)
			return 0;
	}
	for (i = atlas->nnodes; i > idx; i--)
		atlas->nodes[i] = atlas->nodes[i-1];
	atlas->nodes[idx].x = (short)x;
	atlas->nodes[idx].y = (short)y;
	atlas->nodes[idx].width = (short)w;
	atlas->nnodes++;

	return 1;
}

static void fons__atlasRemoveNode(FONSatlas* atlas, int idx)
{
	int i;
	if (atlas->nnodes == 0) return;
	for (i = idx; i < atlas->nnodes-1; i++)
		atlas->nodes[i] = atlas->nodes[i+1];
	atlas->nnodes--;
}

static void fons__atlasExpand(FONSatlas* atlas, int w, int h)
{
	// Insert node for empty space
	if (w > atlas->width)
		fons__atlasInsertNode(atlas, atlas->nnodes, atlas->width, 0, w - atlas->width);
	atlas->width = w;
	atlas->height = h;
}

static void fons__atlasReset(FONSatlas* atlas, int w, int h)
{
	atlas->width = w;
	atlas->height = h;
	atlas->nnodes = 0;

	// Init root node.
	atlas->nodes[0].x = 0;
	atlas->nodes[0].y = 0;
	atlas->nodes[0].width = (short)w;
	atlas->nnodes++;
}

static int fons__atlasAddSkylineLevel(FONSatlas* atlas, int idx, int x, int y, int w, int h)
{
	int i;

	// Insert new node
	if (fons__atlasInsertNode(atlas, idx, x, y+h, w) == 0)
		return 0;

	// Delete skyline segments that fall under the shadow of the new segment.
	for (i = idx+1; i < atlas->nnodes; i++) {
		if (atlas->nodes[i].x < atlas->nodes[i-1].x + atlas->nodes[i-1].width) {
			int shrink = atlas->nodes[i-1].x + atlas->nodes[i-1].width - atlas->nodes[i].x;
			atlas->nodes[i].x += (short)shrink;
			atlas->nodes[i].width -= (short)shrink;
			if (atlas->nodes[i].width <= 0) {
				fons__atlasRemoveNode(atlas, i);
				i--;
			} else {
				break;
			}
		} else {
			break;
		}
	}

	// Merge same height skyline segments that are next to each other.
	for (i = 0; i < atlas->nnodes-1; i++) {
		if (atlas->nodes[i].y == atlas->nodes[i+1].y) {
			atlas->nodes[i].width += atlas->nodes[i+1].width;
			fons__atlasRemoveNode(atlas, i+1);
			i--;
		}
	}

	return 1;
}

static int fons__atlasRectFits(FONSatlas* atlas, int i, int w, int h)
{
	// Checks if there is enough space at the location of skyline span 'i',
	// and return the max height of all skyline spans under that at that location,
	// (think tetris block being dropped at that position). Or -1 if no space found.
	int x = atlas->nodes[i].x;
	int y = atlas->nodes[i].y;
	int spaceLeft;
	if (x + w > atlas->width)
		return -1;
	spaceLeft = w;
	while (spaceLeft > 0) {
		if (i == atlas->nnodes) return -1;
		y = fons__maxi(y, atlas->nodes[i].y);
		if (y + h > atlas->height) return -1;
		spaceLeft -= atlas->nodes[i].width;
		++i;
	}
	return y;
}

static int fons__atlasAddRect(FONSatlas* atlas, int rw, int rh, int* rx, int* ry)
{
	int besth = atlas->height, bestw = atlas->width, besti = -1;
	int bestx = -1, besty = -1, i;

	// Bottom left fit heuristic.
	for (i = 0; i < atlas->nnodes; i++) {
		int y = fons__atlasRectFits(atlas, i, rw, rh);
		if (y != -1) {
			if (y + rh < besth || (y + rh == besth && atlas->nodes[i].width < bestw)) {
				besti = i;
				bestw = atlas->nodes[i].width;
				besth = y + rh;
				bestx = atlas->nodes[i].x;
				besty = y;
			}
		}
	}

	if (besti == -1)
		return 0;

	// Perform the actual packing.
	if (fons__atlasAddSkylineLevel(atlas, besti, bestx, besty, rw, rh) == 0)
		return 0;

	*rx = bestx;
	*ry = besty;

	return 1;
}

static void fons__addWhiteRect(FONScontext* cstash, int w, int h)
{
	int x, y, gx, gy;
	unsigned char* dst;
    FONScontextImpl *stash = cstash->ctxImpl;
	if (fons__atlasAddRect(stash->atlas, w, h, &gx, &gy) == 0)
		return;

	// Rasterize
	dst = &stash->texData[gx + gy * cstash->params.width];
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++)
			dst[x] = 0xff;
		dst += cstash->params.width;
	}

	stash->dirtyRect[0] = fons__mini(stash->dirtyRect[0], gx);
	stash->dirtyRect[1] = fons__mini(stash->dirtyRect[1], gy);
	stash->dirtyRect[2] = fons__maxi(stash->dirtyRect[2], gx+w);
	stash->dirtyRect[3] = fons__maxi(stash->dirtyRect[3], gy+h);
}

FONScontext *fonsCreateInternalFrom(FONSparams *params, FONScontext *other){
    FONScontext* cstash = NULL;
    cstash = (FONScontext*)malloc(sizeof(FONScontext));
	if (cstash == NULL) goto error;
    cstash->params = *params;
    cstash->ctxImpl = other->ctxImpl;
    return cstash;
error:
    if(cstash) free(cstash);
    return NULL;
}

FONScontext* fonsCreateInternal(FONSparams* params)
{
	FONScontext* cstash = NULL;
    FONScontextImpl *stash = NULL;
	// Allocate memory for the font stash.
	cstash = (FONScontext*)malloc(sizeof(FONScontext));
	if (cstash == NULL) goto error;
	memset(cstash, 0, sizeof(FONScontext));
    cstash->ctxImpl = (FONScontextImpl *)malloc(sizeof(FONScontextImpl));
    if (cstash->ctxImpl == NULL) goto error;
    memset(cstash->ctxImpl, 0, sizeof(FONScontext));

    stash = cstash->ctxImpl;
	cstash->params = *params;

	// Allocate scratch buffer.
	stash->scratch = (unsigned char*)malloc(FONS_SCRATCH_BUF_SIZE);
	if (stash->scratch == NULL) goto error;

	// Initialize implementation library
	if (!fons__tt_init(stash)) goto error;

	if (cstash->params.renderCreate != NULL) {
		if (cstash->params.renderCreate(cstash->params.userPtr, cstash->params.width, cstash->params.height) == 0)
			goto error;
	}

	stash->atlas = fons__allocAtlas(cstash->params.width, cstash->params.height, FONS_INIT_ATLAS_NODES);
	if (stash->atlas == NULL) goto error;

	// Allocate space for fonts.
	stash->fonts = (FONSfont**)malloc(sizeof(FONSfont*) * FONS_INIT_FONTS);
	if (stash->fonts == NULL) goto error;
	memset(stash->fonts, 0, sizeof(FONSfont*) * FONS_INIT_FONTS);
	stash->cfonts = FONS_INIT_FONTS;
	stash->nfonts = 0;

	// Create texture for the cache.
	stash->itw = 1.0f/cstash->params.width;
	stash->ith = 1.0f/cstash->params.height;
	stash->texData = (unsigned char*)malloc(cstash->params.width * cstash->params.height);
	if (stash->texData == NULL) goto error;
	memset(stash->texData, 0, cstash->params.width * cstash->params.height);

	stash->dirtyRect[0] = cstash->params.width;
	stash->dirtyRect[1] = cstash->params.height;
	stash->dirtyRect[2] = 0;
	stash->dirtyRect[3] = 0;

	// Add white rect at 0,0 for debug drawing.
	fons__addWhiteRect(cstash, 2,2);

	fonsPushState(cstash);
	fonsClearState(cstash);

	return cstash;

error:
	if(cstash){
        fonsDeleteInternal(cstash);
        free(cstash);
    }
	return NULL;
}

static FONSstate* fons__getState(FONScontextImpl* stash)
{
	return &stash->states[stash->nstates-1];
}

int fonsAddFallbackFont(FONScontext* cstash, int base, int fallback)
{
    FONScontextImpl *stash = cstash->ctxImpl;
	FONSfont* baseFont = stash->fonts[base];
	if (baseFont->nfallbacks < FONS_MAX_FALLBACKS) {
		baseFont->fallbacks[baseFont->nfallbacks++] = fallback;
		return 1;
	}
	return 0;
}

void fonsSetSize(FONScontext* cstash, float size)
{
    FONScontextImpl *stash = cstash->ctxImpl;
	fons__getState(stash)->size = size;
}

void fonsSetColor(FONScontext* cstash, unsigned int color)
{
    FONScontextImpl *stash = cstash->ctxImpl;
	fons__getState(stash)->color = color;
}

void fonsSetSpacing(FONScontext* cstash, float spacing)
{
    FONScontextImpl *stash = cstash->ctxImpl;
	fons__getState(stash)->spacing = spacing;
}

void fonsSetBlur(FONScontext* cstash, float blur)
{
    FONScontextImpl *stash = cstash->ctxImpl;
	fons__getState(stash)->blur = blur;
}

void fonsSetAlign(FONScontext* cstash, int align)
{
    FONScontextImpl *stash = cstash->ctxImpl;
	fons__getState(stash)->align = align;
}

void fonsSetFont(FONScontext* cstash, int font)
{
    FONScontextImpl *stash = cstash->ctxImpl;
	fons__getState(stash)->font = font;
}

void fonsPushState(FONScontext* cstash)
{
    FONScontextImpl *stash = cstash->ctxImpl;
	if (stash->nstates >= FONS_MAX_STATES) {
		if (stash->handleError)
			stash->handleError(stash->errorUptr, FONS_STATES_OVERFLOW, 0);
		return;
	}
	if (stash->nstates > 0)
		memcpy(&stash->states[stash->nstates], &stash->states[stash->nstates-1], sizeof(FONSstate));
	stash->nstates++;
}

void fonsPopState(FONScontext* cstash)
{
    FONScontextImpl *stash = cstash->ctxImpl;
	if (stash->nstates <= 1) {
		if (stash->handleError)
			stash->handleError(stash->errorUptr, FONS_STATES_UNDERFLOW, 0);
		return;
	}
	stash->nstates--;
}

void fonsClearState(FONScontext* cstash)
{
    FONScontextImpl *stash = cstash->ctxImpl;
	FONSstate* state = fons__getState(stash);
	state->size = 12.0f;
	state->color = 0xffffffff;
	state->font = 0;
	state->blur = 0;
	state->spacing = 0;
	state->align = FONS_ALIGN_LEFT | FONS_ALIGN_BASELINE;
}

static void fons__freeFont(FONSfont* font)
{
	if (font == NULL) return;
	if (font->glyphs) free(font->glyphs);
	if (font->freeData && font->data) free(font->data);
	free(font);
}

static int fons__allocFont(FONScontextImpl* stash)
{
	FONSfont* font = NULL;
	if (stash->nfonts+1 > stash->cfonts) {
		stash->cfonts = stash->cfonts == 0 ? 8 : stash->cfonts * 2;
		stash->fonts = (FONSfont**)realloc(stash->fonts, sizeof(FONSfont*) * stash->cfonts);
		if (stash->fonts == NULL)
			return -1;
	}
	font = (FONSfont*)malloc(sizeof(FONSfont));
	if (font == NULL) goto error;
	memset(font, 0, sizeof(FONSfont));

	font->glyphs = (FONSglyph*)malloc(sizeof(FONSglyph) * FONS_INIT_GLYPHS);
	if (font->glyphs == NULL) goto error;
	font->cglyphs = FONS_INIT_GLYPHS;
	font->nglyphs = 0;

	stash->fonts[stash->nfonts++] = font;
	return stash->nfonts-1;

    error:
	fons__freeFont(font);

	return FONS_INVALID;
}

static FILE* fons__fopen(const char* filename, const char* mode)
{
#ifdef _WIN32
	int len = 0;
	int fileLen = strlen(filename);
	int modeLen = strlen(mode);
	wchar_t wpath[MAX_PATH];
	wchar_t wmode[MAX_PATH];
	FILE* f;

	if (fileLen == 0)
		return NULL;
	if (modeLen == 0)
		return NULL;
	len = MultiByteToWideChar(CP_UTF8, 0, filename, fileLen, wpath, fileLen);
	if (len >= MAX_PATH)
		return NULL;
	wpath[len] = L'\0';
	len = MultiByteToWideChar(CP_UTF8, 0, mode, modeLen, wmode, modeLen);
	if (len >= MAX_PATH)
		return NULL;
	wmode[len] = L'\0';
	f = _wfopen(wpath, wmode);
	return f;
#else
	return fopen(filename, mode);
#endif
}

int fonsAddFont(FONScontext* cstash, const char* name, const char* path)
{
    FONSsdfSettings sdfSettings;
	memset(&sdfSettings, 0, sizeof(FONSsdfSettings));
	sdfSettings.sdfEnabled = 0;

	return fonsAddFontSdf(cstash, name, path, sdfSettings);
}

int fonsAddFontSdf(FONScontext* cstash, const char* name, const char* path, FONSsdfSettings sdfSettings)
{
	FILE* fp = 0;
	int dataSize = 0, readed;
	unsigned char* data = NULL;

	// Read in the font data.
	fp = fons__fopen(path, "rb");
	if (fp == NULL) goto error;
	fseek(fp,0,SEEK_END);
	dataSize = (int)ftell(fp);
	fseek(fp,0,SEEK_SET);
	data = (unsigned char*)malloc(dataSize);
	if (data == NULL) goto error;
	readed = fread(data, 1, dataSize, fp);
	fclose(fp);
	fp = 0;
	if (readed != dataSize) goto error;

	return fonsAddFontSdfMem(cstash, name, data, dataSize, 1, sdfSettings);

    error:
	if (data) free(data);
	if (fp) fclose(fp);
	return FONS_INVALID;
}

int fonsAddFontMem(FONScontext* cstash, const char* name, unsigned char* data, int dataSize, int freeData)
{
	FONSsdfSettings sdfSettings;
	memset(&sdfSettings, 0, sizeof(FONSsdfSettings));
	sdfSettings.sdfEnabled = 0;

	return fonsAddFontSdfMem(cstash, name, data, dataSize, freeData, sdfSettings);
}


int fonsAddFontSdfMem(FONScontext* cstash, const char* name, unsigned char* data, int dataSize, int freeData, FONSsdfSettings sdfSettings)
{
	int i, ascent, descent, fh, lineGap;
	FONSfont* font;
    FONScontextImpl *stash = cstash->ctxImpl;

	int idx = fons__allocFont(stash);
	if (idx == FONS_INVALID)
		return FONS_INVALID;

	font = stash->fonts[idx];
	font->sdfSettings = sdfSettings;

	strncpy(font->name, name, sizeof(font->name));
	font->name[sizeof(font->name)-1] = '\0';

	// Init hash lookup.
	for (i = 0; i < FONS_HASH_LUT_SIZE; ++i)
		font->lut[i] = -1;

	// Read in the font data.
	font->dataSize = dataSize;
	font->data = data;
	font->freeData = (unsigned char)freeData;

	// Init font
	stash->nscratch = 0;
	if (!fons__tt_loadFont(stash, &font->font, data, dataSize)) goto error;

	// Store normalized line height. The real line height is got
	// by multiplying the lineh by font size.
	fons__tt_getFontVMetrics( &font->font, &ascent, &descent, &lineGap);
	fh = ascent - descent;
	font->ascender = (float)ascent / (float)fh;
	font->descender = (float)descent / (float)fh;
	font->lineh = (float)(fh + lineGap) / (float)fh;

	return idx;

    error:
	fons__freeFont(font);
	stash->nfonts--;
	return FONS_INVALID;
}

int fonsGetFontByName(FONScontext* cs, const char* name)
{
	int i;
    FONScontextImpl *s = cs->ctxImpl;
	for (i = 0; i < s->nfonts; i++) {
		if (strcmp(s->fonts[i]->name, name) == 0)
			return i;
	}
	return FONS_INVALID;
}


static FONSglyph* fons__allocGlyph(FONSfont* font)
{
	if (font->nglyphs+1 > font->cglyphs) {
		font->cglyphs = font->cglyphs == 0 ? 8 : font->cglyphs * 2;
		font->glyphs = (FONSglyph*)realloc(font->glyphs, sizeof(FONSglyph) * font->cglyphs);
		if (font->glyphs == NULL) return NULL;
	}
	font->nglyphs++;
	return &font->glyphs[font->nglyphs-1];
}


// Based on Exponential blur, Jani Huhtanen, 2006

#define APREC 16
#define ZPREC 7

static void fons__blurCols(unsigned char* dst, int w, int h, int dstStride, int alpha)
{
	int x, y;
	for (y = 0; y < h; y++) {
		int z = 0; // force zero border
		for (x = 1; x < w; x++) {
			z += (alpha * (((int)(dst[x]) << ZPREC) - z)) >> APREC;
			dst[x] = (unsigned char)(z >> ZPREC);
		}
		dst[w-1] = 0; // force zero border
		z = 0;
		for (x = w-2; x >= 0; x--) {
			z += (alpha * (((int)(dst[x]) << ZPREC) - z)) >> APREC;
			dst[x] = (unsigned char)(z >> ZPREC);
		}
		dst[0] = 0; // force zero border
		dst += dstStride;
	}
}

static void fons__blurRows(unsigned char* dst, int w, int h, int dstStride, int alpha)
{
	int x, y;
	for (x = 0; x < w; x++) {
		int z = 0; // force zero border
		for (y = dstStride; y < h*dstStride; y += dstStride) {
			z += (alpha * (((int)(dst[y]) << ZPREC) - z)) >> APREC;
			dst[y] = (unsigned char)(z >> ZPREC);
		}
		dst[(h-1)*dstStride] = 0; // force zero border
		z = 0;
		for (y = (h-2)*dstStride; y >= 0; y -= dstStride) {
			z += (alpha * (((int)(dst[y]) << ZPREC) - z)) >> APREC;
			dst[y] = (unsigned char)(z >> ZPREC);
		}
		dst[0] = 0; // force zero border
		dst++;
	}
}


static void fons__blur(FONScontextImpl* stash, unsigned char* dst, int w, int h, int dstStride, int blur)
{
	int alpha;
	float sigma;
	(void)stash;

	if (blur < 1)
		return;
	// Calculate the alpha such that 90% of the kernel is within the radius. (Kernel extends to infinity)
	sigma = (float)blur * 0.57735f; // 1 / sqrt(3)
	alpha = (int)((1<<APREC) * (1.0f - expf(-2.3f / (sigma+1.0f))));
	fons__blurRows(dst, w, h, dstStride, alpha);
	fons__blurCols(dst, w, h, dstStride, alpha);
	fons__blurRows(dst, w, h, dstStride, alpha);
	fons__blurCols(dst, w, h, dstStride, alpha);
    //	fons__blurrows(dst, w, h, dstStride, alpha);
    //	fons__blurcols(dst, w, h, dstStride, alpha);
}

static FONSglyph* fons__getGlyph(FONScontext* cstash, FONSfont* font, unsigned int codepoint,
								 short isize, short iblur)
{
	int i, g, advance, lsb, x0, y0, x1, y1, gw, gh, gx, gy, x, y;
	float scale;
	FONSglyph* glyph = NULL;
	unsigned int h;
	float size = isize/10.0f;
	int pad, added;
	unsigned char* bdst;
	unsigned char* dst;
	FONSfont* renderFont = font;
    FONScontextImpl *stash = cstash->ctxImpl;

	if (isize < 2) return NULL;
	if (iblur > 20) iblur = 20;
	pad = iblur+2;

	// Reset allocator.
	stash->nscratch = 0;

	// Find code point and size.
	h = fons__hashint(codepoint) & (FONS_HASH_LUT_SIZE-1);
	i = font->lut[h];
	while (i != -1) {
		if (font->glyphs[i].codepoint == codepoint && font->glyphs[i].size == isize && font->glyphs[i].blur == iblur){
			return &font->glyphs[i];
        }
		i = font->glyphs[i].next;
	}

	// Could not find glyph, create it.
	g = fons__tt_getGlyphIndex(&font->font, codepoint);
	// Try to find the glyph in fallback fonts.
	if (g == 0) {
		for (i = 0; i < font->nfallbacks; ++i) {
			FONSfont* fallbackFont = stash->fonts[font->fallbacks[i]];
			int fallbackIndex = fons__tt_getGlyphIndex(&fallbackFont->font, codepoint);
			if (fallbackIndex != 0) {
				g = fallbackIndex;
				renderFont = fallbackFont;
				break;
			}
		}
		// It is possible that we did not find a fallback glyph.
		// In that case the glyph index 'g' is 0, and we'll proceed below and cache empty glyph.
	}
	scale = fons__tt_getPixelHeightScale(&renderFont->font, size);
	fons__tt_buildGlyphBitmap(&renderFont->font, g, size, scale, &advance, &lsb, &x0, &y0, &x1, &y1, &renderFont->sdfSettings);
	gw = x1-x0 + pad*2;
	gh = y1-y0 + pad*2;

	// Find free spot for the rect in the atlas
	added = fons__atlasAddRect(stash->atlas, gw, gh, &gx, &gy);
	if (added == 0 && stash->handleError != NULL) {
		// Atlas is full, let the user to resize the atlas (or not), and try again.
		stash->handleError(stash->errorUptr, FONS_ATLAS_FULL, 0);
		added = fons__atlasAddRect(stash->atlas, gw, gh, &gx, &gy);
	}
	if (added == 0) return NULL;

	// Init glyph.
	glyph = fons__allocGlyph(font);
	glyph->codepoint = codepoint;
	glyph->size = isize;
	glyph->blur = iblur;
	glyph->index = g;
	glyph->x0 = (short)gx;
	glyph->y0 = (short)gy;
	glyph->x1 = (short)(glyph->x0+gw);
	glyph->y1 = (short)(glyph->y0+gh);
	glyph->xadv = (short)(scale * advance * 10.0f);
	glyph->xoff = (short)(x0 - pad);
	glyph->yoff = (short)(y0 - pad);
	glyph->next = 0;

	// Insert char to hash lookup.
	glyph->next = font->lut[h];
	font->lut[h] = font->nglyphs-1;

	// Rasterize
	dst = &stash->texData[(glyph->x0+pad) + (glyph->y0+pad) * cstash->params.width];
	fons__tt_renderGlyphBitmap(&renderFont->font, dst, gw-pad*2,gh-pad*2, cstash->params.width, scale,scale, g, &renderFont->sdfSettings);

	// Make sure there is one pixel empty border.
	dst = &stash->texData[glyph->x0 + glyph->y0 * cstash->params.width];
	for (y = 0; y < gh; y++) {
		dst[y*cstash->params.width] = 0;
		dst[gw-1 + y*cstash->params.width] = 0;
	}
	for (x = 0; x < gw; x++) {
		dst[x] = 0;
		dst[x + (gh-1)*cstash->params.width] = 0;
	}

	// Debug code to color the glyph background
    /*	unsigned char* fdst = &stash->texData[glyph->x0 + glyph->y0 * stash->params.width];
        for (y = 0; y < gh; y++) {
            for (x = 0; x < gw; x++) {
                int a = (int)fdst[x+y*stash->params.width] + 20;
                if (a > 255) a = 255;
                fdst[x+y*stash->params.width] = a;
            }
        }*/

	// Blur
	if (iblur > 0) {
		stash->nscratch = 0;
		bdst = &stash->texData[glyph->x0 + glyph->y0 * cstash->params.width];
		fons__blur(stash, bdst, gw,gh, cstash->params.width, iblur);
	}

	stash->dirtyRect[0] = fons__mini(stash->dirtyRect[0], glyph->x0);
	stash->dirtyRect[1] = fons__mini(stash->dirtyRect[1], glyph->y0);
	stash->dirtyRect[2] = fons__maxi(stash->dirtyRect[2], glyph->x1);
	stash->dirtyRect[3] = fons__maxi(stash->dirtyRect[3], glyph->y1);

	return glyph;
}

static void fons__getQuad(FONScontext* cstash, FONSfont* font,
                          int prevGlyphIndex, FONSglyph* glyph,
                          float scale, float spacing, float* x, float* y, FONSquad* q)
{
	float rx,ry,xoff,yoff,x0,y0,x1,y1;
    FONScontextImpl *stash = cstash->ctxImpl;

	if (prevGlyphIndex != -1) {
		float adv = fons__tt_getGlyphKernAdvance(&font->font, prevGlyphIndex, glyph->index) * scale;
		*x += (int)(adv + spacing + 0.5f);
	}

	// Each glyph has 2px border to allow good interpolation,
	// one pixel to prevent leaking, and one to allow good interpolation for rendering.
	// Inset the texture region by one pixel for correct interpolation.
	xoff = (short)(glyph->xoff+1);
	yoff = (short)(glyph->yoff+1);
	x0 = (float)(glyph->x0+1);
	y0 = (float)(glyph->y0+1);
	x1 = (float)(glyph->x1-1);
	y1 = (float)(glyph->y1-1);

	if (cstash->params.flags & FONS_ZERO_TOPLEFT) {
		rx = (float)(int)(*x + xoff);
		ry = (float)(int)(*y + yoff);

		q->x0 = rx;
		q->y0 = ry;
		q->x1 = rx + x1 - x0;
		q->y1 = ry + y1 - y0;

		q->s0 = x0 * stash->itw;
		q->t0 = y0 * stash->ith;
		q->s1 = x1 * stash->itw;
		q->t1 = y1 * stash->ith;
	} else {
		rx = (float)(int)(*x + xoff);
		ry = (float)(int)(*y - yoff);

		q->x0 = rx;
		q->y0 = ry;
		q->x1 = rx + x1 - x0;
		q->y1 = ry - y1 + y0;

		q->s0 = x0 * stash->itw;
		q->t0 = y0 * stash->ith;
		q->s1 = x1 * stash->itw;
		q->t1 = y1 * stash->ith;
	}

	*x += (int)(glyph->xadv / 10.0f + 0.5f);
}

static void fons__flush(FONScontext* cstash)
{
	// Flush texture
    FONScontextImpl *stash = cstash->ctxImpl;
	if (stash->dirtyRect[0] < stash->dirtyRect[2] && stash->dirtyRect[1] < stash->dirtyRect[3]) {
		if (cstash->params.renderUpdate != NULL)
			cstash->params.renderUpdate(cstash->params.userPtr, stash->dirtyRect, stash->texData);
		// Reset dirty rect
		stash->dirtyRect[0] = cstash->params.width;
		stash->dirtyRect[1] = cstash->params.height;
		stash->dirtyRect[2] = 0;
		stash->dirtyRect[3] = 0;
	}

	// Flush triangles
	if (stash->nverts > 0) {
		if (cstash->params.renderDraw != NULL)
			cstash->params.renderDraw(cstash->params.userPtr, stash->verts, stash->tcoords, stash->colors, stash->nverts);
		stash->nverts = 0;
	}
}

static __inline void fons__vertex(FONScontextImpl* stash, float x, float y, float s, float t, unsigned int c)
{
	stash->verts[stash->nverts*2+0] = x;
	stash->verts[stash->nverts*2+1] = y;
	stash->tcoords[stash->nverts*2+0] = s;
	stash->tcoords[stash->nverts*2+1] = t;
	stash->colors[stash->nverts] = c;
	stash->nverts++;
}

static float fons__getVertAlign(FONScontext* cstash, FONSfont* font, int align, short isize)
{
	if (cstash->params.flags & FONS_ZERO_TOPLEFT) {
		if (align & FONS_ALIGN_TOP) {
			return font->ascender * (float)isize/10.0f;
		} else if (align & FONS_ALIGN_MIDDLE) {
			return (font->ascender + font->descender) / 2.0f * (float)isize/10.0f;
		} else if (align & FONS_ALIGN_BASELINE) {
			return 0.0f;
		} else if (align & FONS_ALIGN_BOTTOM) {
			return font->descender * (float)isize/10.0f;
		}
	} else {
		if (align & FONS_ALIGN_TOP) {
			return -font->ascender * (float)isize/10.0f;
		} else if (align & FONS_ALIGN_MIDDLE) {
			return -(font->ascender + font->descender) / 2.0f * (float)isize/10.0f;
		} else if (align & FONS_ALIGN_BASELINE) {
			return 0.0f;
		} else if (align & FONS_ALIGN_BOTTOM) {
			return -font->descender * (float)isize/10.0f;
		}
	}
	return 0.0;
}

FONS_DEF int fonsComputeStringOffsetCount(FONScontext* cstash, const char *e, float x1){
    FONScontextImpl *stash = cstash->ctxImpl;
    FONSstate* state = fons__getState(stash);
    unsigned int codepoint;
	unsigned int utf8state = 0;
	FONSglyph* glyph = NULL;
	FONSquad q;
	int prevGlyphIndex = -1;
    short isize = (short)(state->size*10.0f);
	short iblur = (short)state->blur;
	float scale;
	FONSfont* font;
	float width;
    float x = 0, y = 0;
    int curr = 0;
    int tabLen = AppGetTabLength(nullptr);
    char *str = (char *)e;
    char *end = (char *)e + strlen(e);

    if (stash == NULL || str == NULL) return 0;
	if (state->font < 0 || state->font >= stash->nfonts) return 0;
	font = stash->fonts[state->font];
	if (font->data == NULL) return 0;

    scale = fons__tt_getPixelHeightScale(&font->font, (float)isize/10.0f);
    // Align horizontally
	if (state->align & FONS_ALIGN_LEFT) {
		// empty
	} else if (state->align & FONS_ALIGN_RIGHT) {
		width = fonsTextBounds(cstash, x,y, str, end, NULL);
		x -= width;
	} else if (state->align & FONS_ALIGN_CENTER) {
		width = fonsTextBounds(cstash, x,y, str, end, NULL);
		x -= width * 0.5f;
	}

    // Align vertically.
	y += fons__getVertAlign(cstash, font, state->align, isize);
    while(str != end){
        char target = *str;
        if(target == '\t'){
            target = ' ';
            for(int ss = 0; ss < tabLen; ss++){
                if (fons__decutf8(&utf8state, &codepoint, (const unsigned char)target))
                    continue;
                glyph = fons__getGlyph(cstash, font, codepoint, isize, iblur);
                if(glyph != NULL){
                    fons__getQuad(cstash, font, prevGlyphIndex, glyph, scale,
                                  state->spacing, &x, &y, &q);
                    x += FONS_DEFAULT_X_SPACING;
                    if(x > x1) return curr;
                }
                prevGlyphIndex = glyph != NULL ? glyph->index : -1;
            }
            curr++;
        }else{
            if (fons__decutf8(&utf8state, &codepoint, (const unsigned char)target)){
                ++str;
                continue;
            }
            glyph = fons__getGlyph(cstash, font, codepoint, isize, iblur);
            if(glyph != NULL){
                fons__getQuad(cstash, font, prevGlyphIndex, glyph, scale,
                              state->spacing, &x, &y, &q);
                x += FONS_DEFAULT_X_SPACING;
                if(x > x1) return curr;
            }
            curr++;
            prevGlyphIndex = glyph != NULL ? glyph->index : -1;
        }
        ++str;
    }
#if 0
    for(; str != end; ++str){
        if (fons__decutf8(&utf8state, &codepoint, *(const unsigned char*)str))
			continue;
        glyph = fons__getGlyph(cstash, font, codepoint, isize, iblur);
        if(glyph != NULL){
            fons__getQuad(cstash, font, prevGlyphIndex, glyph, scale,
                          state->spacing, &x, &y, &q);
            x += FONS_DEFAULT_X_SPACING;
            if(x > x1) return curr;
        }

        curr++;
        prevGlyphIndex = glyph != NULL ? glyph->index : -1;
    }
#endif
    return curr;
}

int AppGetTabLength(int *);
FONS_DEF float fonsComputeStringAdvance(FONScontext* cstash, const char *e, int n,
                                        int *prevGlyph)
{
    FONScontextImpl *stash = cstash->ctxImpl;
    FONSstate* state = fons__getState(stash);
    unsigned int codepoint;
	unsigned int utf8state = 0;
	FONSglyph* glyph = NULL;
	FONSquad q;
	int prevGlyphIndex = *prevGlyph;
    short isize = (short)(state->size*10.0f);
	short iblur = (short)state->blur;
	float scale;
	FONSfont* font;
	float width;
    float x = 0, y = 0;
    char *str = (char *)e;
    char *end = (char *)&str[n];
    int tabLen = AppGetTabLength(nullptr);

    if (stash == NULL || str == NULL) return 0;
	if (state->font < 0 || state->font >= stash->nfonts) return 0;
	font = stash->fonts[state->font];
	if (font->data == NULL) return 0;

    scale = fons__tt_getPixelHeightScale(&font->font, (float)isize/10.0f);
    // Align horizontally
	if (state->align & FONS_ALIGN_LEFT) {
		// empty
	} else if (state->align & FONS_ALIGN_RIGHT) {
		width = fonsTextBounds(cstash, x,y, str, end, NULL);
		x -= width;
	} else if (state->align & FONS_ALIGN_CENTER) {
		width = fonsTextBounds(cstash, x,y, str, end, NULL);
		x -= width * 0.5f;
	}

    // Align vertically.
	y += fons__getVertAlign(cstash, font, state->align, isize);
    int i = 0;
    while(i < n){
        char target = *str;
        if(target == '\t'){
            target = ' ';
            for(int ss = 0; ss < tabLen; ss++){
                if (fons__decutf8(&utf8state, &codepoint, (const unsigned char)target)){
                    continue;
                }

                glyph = fons__getGlyph(cstash, font, codepoint, isize, iblur);
                if(glyph != NULL){
                    fons__getQuad(cstash, font, prevGlyphIndex, glyph, scale,
                                  state->spacing, &x, &y, &q);

                    x += FONS_DEFAULT_X_SPACING;
                }
                prevGlyphIndex = glyph != NULL ? glyph->index : -1;
            }
        }else{
            if (fons__decutf8(&utf8state, &codepoint, (const unsigned char)target)){
                i++;
                ++str;
                continue;
            }

            glyph = fons__getGlyph(cstash, font, codepoint, isize, iblur);
            if(glyph != NULL){
                fons__getQuad(cstash, font, prevGlyphIndex, glyph, scale,
                              state->spacing, &x, &y, &q);

                x += FONS_DEFAULT_X_SPACING;
            }
            prevGlyphIndex = glyph != NULL ? glyph->index : -1;
        }
        i++;
        ++str;
    }
#if 0
    for(int i = 0; i < n; i++, ++str){
        if (fons__decutf8(&utf8state, &codepoint, *(const unsigned char*)str)){
            continue;
        }
        glyph = fons__getGlyph(cstash, font, codepoint, isize, iblur);
        if(glyph != NULL){
            fons__getQuad(cstash, font, prevGlyphIndex, glyph, scale,
                          state->spacing, &x, &y, &q);

            x += FONS_DEFAULT_X_SPACING;
        }

        prevGlyphIndex = glyph != NULL ? glyph->index : -1;
    }
#endif
    *prevGlyph = prevGlyphIndex;

    return x;
}

FONS_DEF int fonsGetGlyphIndex(FONScontext* cstash, int codepoint){
    FONScontextImpl *stash = cstash->ctxImpl;
    FONSstate* state = fons__getState(stash);
    FONSfont* font = stash->fonts[state->font];
	return fons__tt_getGlyphIndex(&font->font, codepoint);
}

FONS_DEF float fonsStashMultiTextColor(FONScontext* cstash,
                                       float x, float y, unsigned int color,
                                       const char* str, const char* end,
                                       int *prevGlyph)
{
    FONScontextImpl *stash = cstash->ctxImpl;
	FONSstate* state = fons__getState(stash);
	unsigned int codepoint;
	unsigned int utf8state = 0;
	FONSglyph* glyph = NULL;
	FONSquad q;
	int prevGlyphIndex = *prevGlyph;
	short isize = (short)(state->size*10.0f);
	short iblur = (short)state->blur;
	float scale;
	FONSfont* font;
	float width;
    int tabLen = AppGetTabLength(nullptr);

	if (stash == NULL || str == NULL) return x;
	if (state->font < 0 || state->font >= stash->nfonts) return x;
	font = stash->fonts[state->font];
	if (font->data == NULL) return x;

	scale = fons__tt_getPixelHeightScale(&font->font, (float)isize/10.0f);

	if (end == NULL)
		end = str + strlen(str);

	// Align horizontally
	if (state->align & FONS_ALIGN_LEFT) {
		// empty
	} else if (state->align & FONS_ALIGN_RIGHT) {
		width = fonsTextBounds(cstash, x,y, str, end, NULL);
		x -= width;
	} else if (state->align & FONS_ALIGN_CENTER) {
		width = fonsTextBounds(cstash, x,y, str, end, NULL);
		x -= width * 0.5f;
	}
	// Align vertically.
	y += fons__getVertAlign(cstash, font, state->align, isize);
    unsigned int tmpColor = color;
    while(str != end){
        color = tmpColor;
        char target = *str;
        if(target == '\t'){
            target = ' ';
            for(int ss = 0; ss < tabLen; ss++){
                if (fons__decutf8(&utf8state, &codepoint, (const unsigned char)target))
                    continue;
                glyph = fons__getGlyph(cstash, font, codepoint, isize, iblur);
                if (glyph != NULL) {
			        fons__getQuad(cstash, font, prevGlyphIndex, glyph,
                                  scale, state->spacing, &x, &y, &q);
                    if (stash->nverts+6 > FONS_VERTEX_COUNT)
				        fons__flush(cstash);
                    fons__vertex(stash, q.x0, q.y0, q.s0, q.t0, color);
			        fons__vertex(stash, q.x1, q.y1, q.s1, q.t1, color);
			        fons__vertex(stash, q.x1, q.y0, q.s1, q.t0, color);

			        fons__vertex(stash, q.x0, q.y0, q.s0, q.t0, color);
			        fons__vertex(stash, q.x0, q.y1, q.s0, q.t1, color);
			        fons__vertex(stash, q.x1, q.y1, q.s1, q.t1, color);

                    x += FONS_DEFAULT_X_SPACING;
                }
                prevGlyphIndex = glyph != NULL ? glyph->index : -1;
            }
        }else{
            unsigned int utf8dec = fons__decutf8(&utf8state, &codepoint,
                                            (const unsigned char)target);
            if(utf8dec){
                ++str;
                continue;
            }

            glyph = fons__getGlyph(cstash, font, codepoint, isize, iblur);
            if (glyph != NULL) {
                fons__getQuad(cstash, font, prevGlyphIndex, glyph,
                              scale, state->spacing, &x, &y, &q);
                if (stash->nverts+6 > FONS_VERTEX_COUNT)
                    fons__flush(cstash);
                fons__vertex(stash, q.x0, q.y0, q.s0, q.t0, color);
                fons__vertex(stash, q.x1, q.y1, q.s1, q.t1, color);
                fons__vertex(stash, q.x1, q.y0, q.s1, q.t0, color);

                fons__vertex(stash, q.x0, q.y0, q.s0, q.t0, color);
                fons__vertex(stash, q.x0, q.y1, q.s0, q.t1, color);
                fons__vertex(stash, q.x1, q.y1, q.s1, q.t1, color);

                x += FONS_DEFAULT_X_SPACING;
            }
            prevGlyphIndex = glyph != NULL ? glyph->index : -1;
        }
        ++str;
    }
#if 0
	for (; str != end; ++str) {
		if (fons__decutf8(&utf8state, &codepoint, *(const unsigned char*)str))
			continue;
        //NOTE: Expensive
		glyph = fons__getGlyph(cstash, font, codepoint, isize, iblur);
		if (glyph != NULL) {
			fons__getQuad(cstash, font, prevGlyphIndex, glyph, scale, state->spacing, &x, &y, &q);

			if (stash->nverts+6 > FONS_VERTEX_COUNT)
				fons__flush(cstash);

			fons__vertex(stash, q.x0, q.y0, q.s0, q.t0, color);
			fons__vertex(stash, q.x1, q.y1, q.s1, q.t1, color);
			fons__vertex(stash, q.x1, q.y0, q.s1, q.t0, color);

			fons__vertex(stash, q.x0, q.y0, q.s0, q.t0, color);
			fons__vertex(stash, q.x0, q.y1, q.s0, q.t1, color);
			fons__vertex(stash, q.x1, q.y1, q.s1, q.t1, color);

            x += FONS_DEFAULT_X_SPACING;
		}
		prevGlyphIndex = glyph != NULL ? glyph->index : -1;
	}
#endif
    *prevGlyph = prevGlyphIndex;

	return x;
}

FONS_DEF void fonsStashFlush(FONScontext *cstash){
    fons__flush(cstash);
}

FONS_DEF float fonsDrawText(FONScontext* cstash,
                            float x, float y,
                            const char* str, const char* end)
{
    FONScontextImpl *stash = cstash->ctxImpl;
	FONSstate* state = fons__getState(stash);
	unsigned int codepoint;
	unsigned int utf8state = 0;
	FONSglyph* glyph = NULL;
	FONSquad q;
	int prevGlyphIndex = -1;
	short isize = (short)(state->size*10.0f);
	short iblur = (short)state->blur;
	float scale;
	FONSfont* font;
	float width;

	if (stash == NULL || str == NULL) return x;
	if (state->font < 0 || state->font >= stash->nfonts) return x;
	font = stash->fonts[state->font];
	if (font->data == NULL) return x;

	scale = fons__tt_getPixelHeightScale(&font->font, (float)isize/10.0f);

	if (end == NULL)
		end = str + strlen(str);

	// Align horizontally
	if (state->align & FONS_ALIGN_LEFT) {
		// empty
	} else if (state->align & FONS_ALIGN_RIGHT) {
		width = fonsTextBounds(cstash, x,y, str, end, NULL);
		x -= width;
	} else if (state->align & FONS_ALIGN_CENTER) {
		width = fonsTextBounds(cstash, x,y, str, end, NULL);
		x -= width * 0.5f;
	}
	// Align vertically.
	y += fons__getVertAlign(cstash, font, state->align, isize);

	for (; str != end; ++str) {
		if (fons__decutf8(&utf8state, &codepoint, *(const unsigned char*)str))
			continue;
		glyph = fons__getGlyph(cstash, font, codepoint, isize, iblur);
		if (glyph != NULL) {
			fons__getQuad(cstash, font, prevGlyphIndex, glyph, scale, state->spacing, &x, &y, &q);

			if (stash->nverts+6 > FONS_VERTEX_COUNT)
				fons__flush(cstash);

			fons__vertex(stash, q.x0, q.y0, q.s0, q.t0, state->color);
			fons__vertex(stash, q.x1, q.y1, q.s1, q.t1, state->color);
			fons__vertex(stash, q.x1, q.y0, q.s1, q.t0, state->color);

			fons__vertex(stash, q.x0, q.y0, q.s0, q.t0, state->color);
			fons__vertex(stash, q.x0, q.y1, q.s0, q.t1, state->color);
			fons__vertex(stash, q.x1, q.y1, q.s1, q.t1, state->color);

            x += FONS_DEFAULT_X_SPACING;
		}
		prevGlyphIndex = glyph != NULL ? glyph->index : -1;
	}
	fons__flush(cstash);

	return x;
}

FONS_DEF int fonsTextIterInit(FONScontext* cstash, FONStextIter* iter,
                              float x, float y, const char* str, const char* end)
{
    FONScontextImpl *stash = cstash->ctxImpl;
	FONSstate* state = fons__getState(stash);
	float width;

	memset(iter, 0, sizeof(*iter));

	if (stash == NULL) return 0;
	if (state->font < 0 || state->font >= stash->nfonts) return 0;
	iter->font = stash->fonts[state->font];
	if (iter->font->data == NULL) return 0;

	iter->isize = (short)(state->size*10.0f);
	iter->iblur = (short)state->blur;
	iter->scale = fons__tt_getPixelHeightScale(&iter->font->font, (float)iter->isize/10.0f);

	// Align horizontally
	if (state->align & FONS_ALIGN_LEFT) {
		// empty
	} else if (state->align & FONS_ALIGN_RIGHT) {
		width = fonsTextBounds(cstash, x,y, str, end, NULL);
		x -= width;
	} else if (state->align & FONS_ALIGN_CENTER) {
		width = fonsTextBounds(cstash, x,y, str, end, NULL);
		x -= width * 0.5f;
	}
	// Align vertically.
	y += fons__getVertAlign(cstash, iter->font, state->align, iter->isize);

	if (end == NULL)
		end = str + strlen(str);

	iter->x = iter->nextx = x;
	iter->y = iter->nexty = y;
	iter->spacing = state->spacing;
	iter->str = str;
	iter->next = str;
	iter->end = end;
	iter->codepoint = 0;
	iter->prevGlyphIndex = -1;

	return 1;
}

FONS_DEF int fonsTextIterNext(FONScontext* cstash, FONStextIter* iter, FONSquad* quad)
{
	FONSglyph* glyph = NULL;
	const char* str = iter->next;
	iter->str = iter->next;

	if (str == iter->end)
		return 0;

	for (; str != iter->end; str++) {
		if (fons__decutf8(&iter->utf8state, &iter->codepoint, *(const unsigned char*)str))
			continue;
		str++;
		// Get glyph and quad
		iter->x = iter->nextx;
		iter->y = iter->nexty;
		glyph = fons__getGlyph(cstash, iter->font, iter->codepoint, iter->isize, iter->iblur);
		if (glyph != NULL)
			fons__getQuad(cstash, iter->font, iter->prevGlyphIndex, glyph, iter->scale, iter->spacing, &iter->nextx, &iter->nexty, quad);
		iter->prevGlyphIndex = glyph != NULL ? glyph->index : -1;
		break;
	}
	iter->next = str;

	return 1;
}

FONS_DEF void fonsDrawDebug(FONScontext* cstash, float x, float y)
{
    FONScontextImpl *stash = cstash->ctxImpl;
	int i;
	int w = cstash->params.width;
	int h = cstash->params.height;
	float u = w == 0 ? 0 : (1.0f / w);
	float v = h == 0 ? 0 : (1.0f / h);

	if (stash->nverts+6+6 > FONS_VERTEX_COUNT)
		fons__flush(cstash);

	// Draw background
	fons__vertex(stash, x+0, y+0, u, v, 0x0fffffff);
	fons__vertex(stash, x+w, y+h, u, v, 0x0fffffff);
	fons__vertex(stash, x+w, y+0, u, v, 0x0fffffff);

	fons__vertex(stash, x+0, y+0, u, v, 0x0fffffff);
	fons__vertex(stash, x+0, y+h, u, v, 0x0fffffff);
	fons__vertex(stash, x+w, y+h, u, v, 0x0fffffff);

	// Draw texture
	fons__vertex(stash, x+0, y+0, 0, 0, 0xffffffff);
	fons__vertex(stash, x+w, y+h, 1, 1, 0xffffffff);
	fons__vertex(stash, x+w, y+0, 1, 0, 0xffffffff);

	fons__vertex(stash, x+0, y+0, 0, 0, 0xffffffff);
	fons__vertex(stash, x+0, y+h, 0, 1, 0xffffffff);
	fons__vertex(stash, x+w, y+h, 1, 1, 0xffffffff);

	// Drawbug draw atlas
	for (i = 0; i < stash->atlas->nnodes; i++) {
		FONSatlasNode* n = &stash->atlas->nodes[i];

		if (stash->nverts+6 > FONS_VERTEX_COUNT)
			fons__flush(cstash);

		fons__vertex(stash, x+n->x+0, y+n->y+0, u, v, 0xc00000ff);
		fons__vertex(stash, x+n->x+n->width, y+n->y+1, u, v, 0xc00000ff);
		fons__vertex(stash, x+n->x+n->width, y+n->y+0, u, v, 0xc00000ff);

		fons__vertex(stash, x+n->x+0, y+n->y+0, u, v, 0xc00000ff);
		fons__vertex(stash, x+n->x+0, y+n->y+1, u, v, 0xc00000ff);
		fons__vertex(stash, x+n->x+n->width, y+n->y+1, u, v, 0xc00000ff);
	}

	fons__flush(cstash);
}

FONS_DEF float fonsTextBounds(FONScontext* cstash,
                              float x, float y,
                              const char* str, const char* end,
                              float* bounds)
{
    FONScontextImpl *stash = cstash->ctxImpl;
	FONSstate* state = fons__getState(stash);
	unsigned int codepoint;
	unsigned int utf8state = 0;
	FONSquad q;
	FONSglyph* glyph = NULL;
	int prevGlyphIndex = -1;
	short isize = (short)(state->size*10.0f);
	short iblur = (short)state->blur;
	float scale;
	FONSfont* font;
	float startx, advance;
	float minx, miny, maxx, maxy;

	if (stash == NULL) return 0;
	if (state->font < 0 || state->font >= stash->nfonts) return 0;
	font = stash->fonts[state->font];
	if (font->data == NULL) return 0;

	scale = fons__tt_getPixelHeightScale(&font->font, (float)isize/10.0f);

	// Align vertically.
	y += fons__getVertAlign(cstash, font, state->align, isize);

	minx = maxx = x;
	miny = maxy = y;
	startx = x;

	if (end == NULL)
		end = str + strlen(str);

	for (; str != end; ++str) {
		if (fons__decutf8(&utf8state, &codepoint, *(const unsigned char*)str))
			continue;
		glyph = fons__getGlyph(cstash, font, codepoint, isize, iblur);
		if (glyph != NULL) {
			fons__getQuad(cstash, font, prevGlyphIndex, glyph, scale, state->spacing, &x, &y, &q);
			if (q.x0 < minx) minx = q.x0;
			if (q.x1 > maxx) maxx = q.x1;
			if (cstash->params.flags & FONS_ZERO_TOPLEFT) {
				if (q.y0 < miny) miny = q.y0;
				if (q.y1 > maxy) maxy = q.y1;
			} else {
				if (q.y1 < miny) miny = q.y1;
				if (q.y0 > maxy) maxy = q.y0;
			}
		}
		prevGlyphIndex = glyph != NULL ? glyph->index : -1;
	}

	advance = x - startx;

	// Align horizontally
	if (state->align & FONS_ALIGN_LEFT) {
		// empty
	} else if (state->align & FONS_ALIGN_RIGHT) {
		minx -= advance;
		maxx -= advance;
	} else if (state->align & FONS_ALIGN_CENTER) {
		minx -= advance * 0.5f;
		maxx -= advance * 0.5f;
	}

	if (bounds) {
		bounds[0] = minx;
		bounds[1] = miny;
		bounds[2] = maxx;
		bounds[3] = maxy;
	}

	return advance;
}

FONS_DEF void fonsVertMetrics(FONScontext* cstash,
                              float* ascender, float* descender, float* lineh)
{
	FONSfont* font;
    FONScontextImpl *stash = cstash->ctxImpl;
	FONSstate* state = fons__getState(stash);
	short isize;

	if (stash == NULL) return;
	if (state->font < 0 || state->font >= stash->nfonts) return;
	font = stash->fonts[state->font];
	isize = (short)(state->size*10.0f);
	if (font->data == NULL) return;

	if (ascender)
		*ascender = font->ascender*isize/10.0f;
	if (descender)
		*descender = font->descender*isize/10.0f;
	if (lineh)
		*lineh = font->lineh*isize/10.0f;
}

FONS_DEF void fonsLineBounds(FONScontext* cstash, float y, float* miny, float* maxy)
{
	FONSfont* font;
    FONScontextImpl *stash = cstash->ctxImpl;
	FONSstate* state = fons__getState(stash);
	short isize;

	if (stash == NULL) return;
	if (state->font < 0 || state->font >= stash->nfonts) return;
	font = stash->fonts[state->font];
	isize = (short)(state->size*10.0f);
	if (font->data == NULL) return;

	y += fons__getVertAlign(cstash, font, state->align, isize);

	if (cstash->params.flags & FONS_ZERO_TOPLEFT) {
		*miny = y - font->ascender * (float)isize/10.0f;
		*maxy = *miny + font->lineh*isize/10.0f;
	} else {
		*maxy = y + font->descender * (float)isize/10.0f;
		*miny = *maxy - font->lineh*isize/10.0f;
	}
}

FONS_DEF const unsigned char* fonsGetTextureData(FONScontext* cstash, int* width, int* height)
{
    FONScontextImpl *stash = cstash->ctxImpl;
	if (width != NULL)
		*width = cstash->params.width;
	if (height != NULL)
		*height = cstash->params.height;
	return stash->texData;
}

FONS_DEF int fonsValidateTexture(FONScontext* cstash, int* dirty)
{
    FONScontextImpl *stash = cstash->ctxImpl;
	if (stash->dirtyRect[0] < stash->dirtyRect[2] && stash->dirtyRect[1] < stash->dirtyRect[3]) {
		dirty[0] = stash->dirtyRect[0];
		dirty[1] = stash->dirtyRect[1];
		dirty[2] = stash->dirtyRect[2];
		dirty[3] = stash->dirtyRect[3];
		// Reset dirty rect
		stash->dirtyRect[0] = cstash->params.width;
		stash->dirtyRect[1] = cstash->params.height;
		stash->dirtyRect[2] = 0;
		stash->dirtyRect[3] = 0;
		return 1;
	}
	return 0;
}

FONS_DEF void fonsDeleteInternal(FONScontext* cstash)
{
	int i;
	if (cstash == NULL) return;
    FONScontextImpl *stash = cstash->ctxImpl;

	if (cstash->params.renderDelete)
		cstash->params.renderDelete(cstash->params.userPtr);

	for (i = 0; i < stash->nfonts; ++i)
		fons__freeFont(stash->fonts[i]);

	if (stash->atlas) fons__deleteAtlas(stash->atlas);
	if (stash->fonts) free(stash->fonts);
	if (stash->texData) free(stash->texData);
	if (stash->scratch) free(stash->scratch);
	free(stash);
}

FONS_DEF void fonsSetErrorCallback(FONScontext* cstash, void (*callback)(void* uptr, int error, int val), void* uptr)
{
	if (cstash == NULL) return;
    FONScontextImpl *stash = cstash->ctxImpl;
	stash->handleError = callback;
	stash->errorUptr = uptr;
}

FONS_DEF void fonsGetAtlasSize(FONScontext* cstash, int* width, int* height)
{
	if (cstash == NULL) return;
	*width = cstash->params.width;
	*height = cstash->params.height;
}

FONS_DEF int fonsExpandAtlas(FONScontext* cstash, int width, int height)
{
	int i, maxy = 0;
	unsigned char* data = NULL;
	if (cstash == NULL) return 0;
    FONScontextImpl *stash = cstash->ctxImpl;
	width = fons__maxi(width, cstash->params.width);
	height = fons__maxi(height, cstash->params.height);

	if (width == cstash->params.width && height == cstash->params.height)
		return 1;

	// Flush pending glyphs.
	fons__flush(cstash);

	// Create new texture
	if (cstash->params.renderResize != NULL) {
		if (cstash->params.renderResize(cstash->params.userPtr, width, height) == 0)
			return 0;
	}
	// Copy old texture data over.
	data = (unsigned char*)malloc(width * height);
	if (data == NULL)
		return 0;
	for (i = 0; i < cstash->params.height; i++) {
		unsigned char* dst = &data[i*width];
		unsigned char* src = &stash->texData[i*cstash->params.width];
		memcpy(dst, src, cstash->params.width);
		if (width > cstash->params.width)
			memset(dst+cstash->params.width, 0, width - cstash->params.width);
	}
	if (height > cstash->params.height)
		memset(&data[cstash->params.height * width], 0, (height - cstash->params.height) * width);

	free(stash->texData);
	stash->texData = data;

	// Increase atlas size
	fons__atlasExpand(stash->atlas, width, height);

	// Add existing data as dirty.
	for (i = 0; i < stash->atlas->nnodes; i++)
		maxy = fons__maxi(maxy, stash->atlas->nodes[i].y);
	stash->dirtyRect[0] = 0;
	stash->dirtyRect[1] = 0;
	stash->dirtyRect[2] = cstash->params.width;
	stash->dirtyRect[3] = maxy;

	cstash->params.width = width;
	cstash->params.height = height;
	stash->itw = 1.0f/cstash->params.width;
	stash->ith = 1.0f/cstash->params.height;

	return 1;
}

FONS_DEF int fonsResetAtlas(FONScontext* cstash, int width, int height)
{
	int i, j;
	if (cstash == NULL) return 0;
    FONScontextImpl *stash = cstash->ctxImpl;

	// Flush pending glyphs.
	fons__flush(cstash);

	// Create new texture
	if (cstash->params.renderResize != NULL) {
		if (cstash->params.renderResize(cstash->params.userPtr, width, height) == 0)
			return 0;
	}

	// Reset atlas
	fons__atlasReset(stash->atlas, width, height);

	// Clear texture data.
	stash->texData = (unsigned char*)realloc(stash->texData, width * height);
	if (stash->texData == NULL) return 0;
	memset(stash->texData, 0, width * height);

	// Reset dirty rect
	stash->dirtyRect[0] = width;
	stash->dirtyRect[1] = height;
	stash->dirtyRect[2] = 0;
	stash->dirtyRect[3] = 0;

	// Reset cached glyphs
	for (i = 0; i < stash->nfonts; i++) {
		FONSfont* font = stash->fonts[i];
		font->nglyphs = 0;
		for (j = 0; j < FONS_HASH_LUT_SIZE; j++)
			font->lut[j] = -1;
	}

	cstash->params.width = width;
	cstash->params.height = height;
	stash->itw = 1.0f/cstash->params.width;
	stash->ith = 1.0f/cstash->params.height;

	// Add white rect at 0,0 for debug drawing.
	fons__addWhiteRect(cstash, 2,2);

	return 1;
}

#endif // FONTSTASH_IMPLEMENTATION
