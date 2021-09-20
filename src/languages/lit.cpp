#include <languages.h>

TokenizerSupport litSupport = {
    .comments = true,
    .strings = true,
    .numbers = true,
    .lookups = true,
    .multilineComment = false,
    .lineCommentChar = '#',
};

// This is kinda hacky because Lit uses '-' symbol for keywords
// for instance: set-integrator is one token. However our lexer
// does not support breaking out the divider list so we hack
// the lexer by letting it split 'set-integrator' into 3 tokens:
// 'set'   +  '-'   + 'integrator'. At somepoint we might
// want to allow for a more robust divider list on the lexer
// but it is not a must now.

/* lit predefined components */
std::vector<std::vector<GToken>> litReservedTable = {
    {
        { .value = "-", .identifier = TOKEN_ID_OPERATOR }, }, // hacky
    {
        { .value = "uv", .identifier = TOKEN_ID_OPERATOR }, },
    {
        { .value = "sky", .identifier = TOKEN_ID_OPERATOR },
        { .value = "ray", .identifier = TOKEN_ID_OPERATOR },
        { .value = "set", .identifier = TOKEN_ID_OPERATOR },
        { .value = "eta", .identifier = TOKEN_ID_OPERATOR },
        { .value = "mat", .identifier = TOKEN_ID_OPERATOR }, },
    {
        { .value = "path", .identifier = TOKEN_ID_OPERATOR },
        { .value = "type", .identifier = TOKEN_ID_OPERATOR },
        { .value = "flip", .identifier = TOKEN_ID_OPERATOR },
        { .value = "base", .identifier = TOKEN_ID_OPERATOR },
        { .value = "name", .identifier = TOKEN_ID_OPERATOR },
        { .value = "View", .identifier = TOKEN_ID_DATATYPE }, },
    {
        { .value = "image", .identifier = TOKEN_ID_OPERATOR },
        { .value = "width", .identifier = TOKEN_ID_OPERATOR },
        { .value = "light", .identifier = TOKEN_ID_OPERATOR },
        { .value = "pixel", .identifier = TOKEN_ID_OPERATOR },
        { .value = "depth", .identifier = TOKEN_ID_OPERATOR },
        { .value = "model", .identifier = TOKEN_ID_OPERATOR },
        { .value = "coeff", .identifier = TOKEN_ID_OPERATOR },
        { .value = "rough", .identifier = TOKEN_ID_OPERATOR },
        { .value = "sigma", .identifier = TOKEN_ID_OPERATOR },
        { .value = "scale", .identifier = TOKEN_ID_OPERATOR },
        { .value = "Shape", .identifier = TOKEN_ID_DATATYPE }, },
    {
        { .value = "sundir", .identifier = TOKEN_ID_OPERATOR },
        { .value = "pixels", .identifier = TOKEN_ID_OPERATOR },
        { .value = "height", .identifier = TOKEN_ID_OPERATOR },
        { .value = "global", .identifier = TOKEN_ID_OPERATOR },
        { .value = "vertex", .identifier = TOKEN_ID_OPERATOR },
        { .value = "normal", .identifier = TOKEN_ID_OPERATOR },
        { .value = "rotate", .identifier = TOKEN_ID_OPERATOR },
        { .value = "radius", .identifier = TOKEN_ID_OPERATOR },
        { .value = "center", .identifier = TOKEN_ID_OPERATOR },
        { .value = "Filter", .identifier = TOKEN_ID_DATATYPE }, },
    {
        { .value = "texture", .identifier = TOKEN_ID_OPERATOR },
        { .value = "Texture", .identifier = TOKEN_ID_DATATYPE },
        { .value = "samples", .identifier = TOKEN_ID_OPERATOR },
        { .value = "indices", .identifier = TOKEN_ID_OPERATOR },
        { .value = "normals", .identifier = TOKEN_ID_OPERATOR },
        { .value = "Include", .identifier = TOKEN_ID_DATATYPE },
        { .value = "include", .identifier = TOKEN_ID_DATATYPE }, },
    {
        { .value = "rayleigh", .identifier = TOKEN_ID_OPERATOR },
        { .value = "emission", .identifier = TOKEN_ID_OPERATOR },
        { .value = "geometry", .identifier = TOKEN_ID_OPERATOR },
        { .value = "Material", .identifier = TOKEN_ID_DATATYPE }, },
    {
        { .value = "intensity", .identifier = TOKEN_ID_OPERATOR },
        { .value = "normalize", .identifier = TOKEN_ID_OPERATOR },
        { .value = "transform", .identifier = TOKEN_ID_OPERATOR },
        { .value = "translate", .identifier = TOKEN_ID_OPERATOR }, },
    {
        { .value = "integrator", .identifier = TOKEN_ID_OPERATOR },
        { .value = "resolution", .identifier = TOKEN_ID_OPERATOR },
        { .value = "dispersion", .identifier = TOKEN_ID_OPERATOR }, },
    {
        { .value = "reflectance", .identifier = TOKEN_ID_OPERATOR }, },
};

std::vector<std::vector<GToken>> litReservedPreprocessor = {
    // Lit does not have any reserved preprocessor
};
