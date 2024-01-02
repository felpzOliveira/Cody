#include <encoding.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

static bool
IsEncoderTypeValid(EncoderDecoderType type){
    return type == ENCODER_DECODER_UTF8 || type == ENCODER_DECODER_LATIN1;
}

////////////////////////////////////////////////////////////////////////
//                            U T F  -  8
////////////////////////////////////////////////////////////////////////

// Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

#define UTF8_ACCEPT 0
#define UTF8_REJECT 1

static const uint8_t utf8d[] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 00..1f
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 20..3f
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 40..5f
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 60..7f
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, // 80..9f
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, // a0..bf
    8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, // c0..df
    0xa,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x4,0x3,0x3, // e0..ef
    0xb,0x6,0x6,0x6,0x5,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8, // f0..ff
    0x0,0x1,0x2,0x3,0x5,0x8,0x7,0x1,0x1,0x1,0x4,0x6,0x1,0x1,0x1,0x1, // s0..s0
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,1, // s1..s2
    1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1, // s3..s4
    1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,3,1,1,1,1,1,1, // s5..s6
    1,3,1,1,1,1,1,3,1,3,1,1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // s7..s8
};

unsigned int StringToCodepointUpdate_UTF8(unsigned int *state, unsigned int *codep,
                                          unsigned int byte)
{
    uint32_t type = utf8d[byte];

    *codep = (*state != UTF8_ACCEPT) ?
            (byte & 0x3fu) | (*codep << 6) :
                (0xff >> type) & (byte);

    *state = utf8d[256 + *state*16 + type];
    return *state;
}

int StringToCodepoint_UTF8(char *u, int size, int *off){
    int l = size;
    *off = 0;
    if(l < 1) return -1;

    unsigned char u0 = u[0];

    if(u0 >= 0 && u0 <= 127){
        *off = 1;
        return u0; // Ascii table
    }

    if(l < 2) return -1; // if not Ascii we don't known what is this

    unsigned char u1 = u[1];
    if(u0 >= 192 && u0 <= 223){
        *off = 2;
        return (u0 - 192) * 64 + (u1 - 128);
    }

    if(u[0] == 0xed && (u[1] & 0xa0) == 0xa0)
        return -1; //code points, 0xd800 to 0xdfff
    if(l < 3) return -1;

    unsigned char u2 = u[2];
    if(u0 >= 224 && u0 <= 239){
        *off = 3;
        return (u0 - 224) * 4096 + (u1 - 128) * 64 + (u2 - 128);
    }

    if(l < 4) return -1;

    unsigned char u3 = u[3];
    if(u0 >= 240 && u0 <= 247){
        *off = 4;
        return (u0 - 240) * 262144 + (u1 - 128) * 4096 + (u2 - 128) * 64 + (u3 - 128);
    }

    return -1;
}

int CodepointToString_UTF8(int cp, char *c){
    int n = -1;
    memset(c, 0x00, sizeof(char) * 5);
    if(cp <= 0x7F){
        c[0] = cp;
        n = 1;
    }else if(cp <= 0x7FF){
        c[0] = (cp >> 6) + 192;
        c[1] = (cp & 63) + 128;
        n = 2;
    }else if(0xd800 <= cp && cp <= 0xdfff){
        n = 0;
        //invalid block of utf8
    }else if(cp <= 0xFFFF){
        c[0] = (cp >> 12) + 224;
        c[1]= ((cp >> 6) & 63) + 128;
        c[2]=(cp & 63) + 128;
        n = 3;
    }else if(cp <= 0x10FFFF){
        c[0] = (cp >> 18) + 240;
        c[1] = ((cp >> 12) & 63) + 128;
        c[2] = ((cp >> 6) & 63) + 128;
        c[3] = (cp & 63) + 128;
        n = 4;
    }
    return n;
}

char *ConvertFromUtf8_UTF8(char *source, int *len){
    return source;
}
////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
//                          L A T I N  -  1
////////////////////////////////////////////////////////////////////////
int StringToCodepoint_Latin1(char *u, int size, int *off){
    if(size < 1) return -1;

    unsigned char u0 = u[0];
    if(u0 >= 0 && u0 <= 127){
        *off = 1;
        return u0; // Ascii table
    }

    // invalid block from 0x7f to 0x9f
    if(u0 <= 159) return -1;

    // everything else is just a single byte with the value
    *off = 1;
    return u0;
}

unsigned int StringToCodepointUpdate_Latin1(unsigned int *state, unsigned int *codepoint,
                                            unsigned int byte)
{
    *codepoint = byte;
    *state = (byte > 0x7f && byte < 0xA0) ? 1 : 0;
    return *state;
}

int CodepointToString_Latin1(int cp, char *c){
    int n = -1;
    memset(c, 0x00, sizeof(char) * 5);
    if(cp <= 0x7F){ // Ascii
        c[0] = cp;
        n = 1;
    }else if(cp >= 0xA0){
        c[0] = cp;
        n = 1;
    }
    return n;
}

char *ConvertFromUtf8_Latin1(char *source, int *len){
    int readIndex = 0;
    int writeIndex = 0;
    while(readIndex < *len){
        int offset = 0;
        int codepoint = StringToCodepoint_UTF8(&source[readIndex],
                                               *len - readIndex, &offset);
        if(codepoint != -1)
            source[writeIndex++] = codepoint & 0xff;

        readIndex += offset;
    }
    source[writeIndex] = 0;
    *len = writeIndex;
    return source;
}

////////////////////////////////////////////////////////////////////////

int StringToCodepoint(EncoderDecoder *encoder, char *u, int size, int *off){
    if(encoder && encoder->stringToCodepointFn){
        return encoder->stringToCodepointFn(u, size, off);
    }

    printf("NIL ENCODER_DECODER OR UNITIALIZED!!\n");
    return -1;
}

unsigned int StringToCodepointUpdate(EncoderDecoder *encoder, unsigned int *state,
                                     unsigned int *codepoint, unsigned int byte)
{
    if(encoder && encoder->stringToCodepointUpdateFn){
        return encoder->stringToCodepointUpdateFn(state, codepoint, byte);
    }

    printf("NIL ENCODER_DECODER OR UNITIALIZED!!\n");
    return -1;
}

char *ConvertFromUTF8(EncoderDecoder *encoder, char *input, int *len){
    if(encoder && encoder->fromUtf8Fn){
        return encoder->fromUtf8Fn(input, len);
    }

    printf("NIL ENCODER_DECODER OR UNITIALIZED!!\n");
    return input;
}

unsigned int StringToCodepointUpdate_Opaque(void *encoder, unsigned int *state,
                                            unsigned int *codepoint, unsigned int byte)
{
    return StringToCodepointUpdate((EncoderDecoder *)encoder, state, codepoint, byte);
}

void EncoderDecoder_InitFor(EncoderDecoder *encoder, EncoderDecoderType type){
    if(!IsEncoderTypeValid(type))
        return;

    switch(type){
        case ENCODER_DECODER_UTF8:{
            encoder->stringToCodepointFn = StringToCodepoint_UTF8;
            encoder->codepointToStringFn = CodepointToString_UTF8;
            encoder->stringToCodepointUpdateFn = StringToCodepointUpdate_UTF8;
            encoder->fromUtf8Fn = ConvertFromUtf8_UTF8;
        } break;
        case ENCODER_DECODER_LATIN1:{
            encoder->stringToCodepointFn = StringToCodepoint_Latin1;
            encoder->codepointToStringFn = CodepointToString_Latin1;
            encoder->stringToCodepointUpdateFn = StringToCodepointUpdate_Latin1;
            encoder->fromUtf8Fn = ConvertFromUtf8_Latin1;
        } break;
        default:{}
    }

    encoder->type = type;
}

EncoderDecoderType EncoderDecoder_GetType(std::string str){
    if(str == "u8" || str == "U8")
        return ENCODER_DECODER_UTF8;
    else if(str == "lat1" || str == "LAT1")
        return ENCODER_DECODER_LATIN1;
    return ENCODER_DECODER_NONE;
}

const char *EncoderName(EncoderDecoder *encoder){
    switch(encoder->type){
        case ENCODER_DECODER_UTF8:{ return "U8"; } break;
        case ENCODER_DECODER_LATIN1:{ return "LAT1"; } break;
        default: return "?";
    }
}

static
EncoderDecoder globalUTF8Encoder(ENCODER_DECODER_UTF8, StringToCodepoint_UTF8,
                                 CodepointToString_UTF8, StringToCodepointUpdate_UTF8,
                                 ConvertFromUtf8_UTF8);

EncoderDecoder *UTF8Encoder(){
    return &globalUTF8Encoder;
}
