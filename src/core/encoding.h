/* date = December 28th 2023 21:38 */
#pragma once
#include <string>

#define ENCODER_DECODER_FN_TO_CODEPOINT(name) int name(char *, int, int *)
#define ENCODER_DECODER_FN_FROM_CODEPOINT(name) int name(int, char *)
#define ENCODER_DECODER_FN_TO_CODEPOINT_UPDATE(name) unsigned int name(unsigned int *, unsigned int *, unsigned int)
#define ENCODER_DECODER_FN_FROM_UTF8(name) char *name(char *, int *)

typedef ENCODER_DECODER_FN_TO_CODEPOINT(EncoderDecoder_StringToCodepoint);
typedef ENCODER_DECODER_FN_FROM_CODEPOINT(EncoderDecoder_CodepointToString);
typedef ENCODER_DECODER_FN_TO_CODEPOINT_UPDATE(EncoderDecoder_StringToCodepointUpdate);
typedef ENCODER_DECODER_FN_FROM_UTF8(EncoderDecoder_ConvertFromUtf8);

// NOTE: Small note, recall that some parts of the editor make direct comparison
//       to a few characters like '\t', '\r', '\n'. So when adding a encoding
//       make sure it supports the basic Ascii table, I try to call the encoder
//       everywhere but there a few places where direct comparison to these characters
//       happen, so we need the values from 0x0 ~ 0x7f to be included as in the Ascii
//       table.

typedef enum{
    ENCODER_DECODER_UTF8=0,
    ENCODER_DECODER_LATIN1,

    ENCODER_DECODER_NONE
}EncoderDecoderType;

struct EncoderDecoder{
    EncoderDecoderType type;
    EncoderDecoder_StringToCodepoint *stringToCodepointFn;
    EncoderDecoder_CodepointToString *codepointToStringFn;
    EncoderDecoder_StringToCodepointUpdate *stringToCodepointUpdateFn;
    EncoderDecoder_ConvertFromUtf8 *fromUtf8Fn;

    EncoderDecoder(): type(ENCODER_DECODER_NONE), stringToCodepointFn(nullptr),
        codepointToStringFn(nullptr), stringToCodepointUpdateFn(nullptr), fromUtf8Fn(nullptr){}

    EncoderDecoder(EncoderDecoderType tp, EncoderDecoder_StringToCodepoint *toCp,
                   EncoderDecoder_CodepointToString *toStr,
                   EncoderDecoder_StringToCodepointUpdate *update,
                   EncoderDecoder_ConvertFromUtf8 *fromU8) :
        type(tp), stringToCodepointFn(toCp), codepointToStringFn(toStr),
        stringToCodepointUpdateFn(update), fromUtf8Fn(fromU8){}
};

/*
* Sets encoder-decoder appropriate values for the target encoding.
*/
void EncoderDecoder_InitFor(EncoderDecoder *encoder, EncoderDecoderType type);

/*
* Gets a simple printable string for representing the encoder.
*/
const char *EncoderName(EncoderDecoder *encoder);

/*
* Gets a type for the encoder given by the string.
*/
EncoderDecoderType EncoderDecoder_GetType(std::string str);

/*
* Given a string 'u' and its length 'size', returns the codepoint related
* to the first character in it, if the character is not valid under the
* encoding selected it returns -1. The variable 'off' returns the length of
* the character so it is possible to chain calls with something like:
*
*    at = 0;
*    while(1){
*        codepoint = StringToCodepoint(encoder, &u[at], size-at, &off);
*        if(codepoint == -1)
*            break;
*        at += off;
*        <process codepoint>
*    }
*/
int StringToCodepoint(EncoderDecoder *encoder, char *u, int size, int *off);

/*
* Converts a string 'input' with size 'len' that is in UTF-8 to whatever the encoder type
* is. This procedure can fail depending on the encoder support, so be aware.
*/
char *ConvertFromUTF8(EncoderDecoder *encoder, char *input, int *len);

/*
* Similar to StringToCodepoint however this routine processes single byte and store
* possible states for next calls. It is a wrapper for font rendering only and should not
* be called elsewhere. It provides a method for generating codepoint on byte basis when
* we don't want to or can't iterate the entire string yet.
* Returns 0 in case codepoint is ready, != 0 if we need more bytes, it is responsability
* of the caller to gather the length of the character in this situation.
*/
unsigned int StringToCodepointUpdate(EncoderDecoder *encoder, unsigned int *state,
                                     unsigned int *codepoint, unsigned int byte);

// Opaque calls so fontstash can ignore data-types
unsigned int StringToCodepointUpdate_Opaque(void *encoder, unsigned int *state,
                                            unsigned int *codepoint, unsigned int byte);

/*
* if you need a encoder-decoder and dont care what type it is and only need it for
* rendering specific stuff, get a UTF-8 one with the following call.
*/
EncoderDecoder *UTF8Encoder();

/*
* Sets the global encoding scheme when opening buffers.
*/
bool SetGlobalDefaultEncoding(std::string str);

/*
* Gets the global encoding scheme when opening buffers.
*/
EncoderDecoderType GetGlobalDefaultEncoding();
