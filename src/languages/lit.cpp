/* Build from '/home/felipe/Documents/Cody/lang_tables/lang_lit' */
/* Contextual content */
/////////////////////////////////
#include <languages.h>

TokenizerSupport litSupport = {
    .comments = true,
    .strings = true,
    .numbers = true,
    .lookups = true,
    .multilineComment = false,
    .lineCommentChar = '#',
    .procs = {},
};

// This is kinda hacky because Lit uses '-' symbol for keywords
// for instance: set-integrator is one token. However our lexer
// does not support breaking out the divider list so we hack
// the lexer by letting it split 'set-integrator' into 3 tokens:
// 'set'   +  '-'   + 'integrator'. At somepoint we might
// want to allow for a more robust divider list on the lexer
// but it is not a must now.


/////////////////////////////////
/* Auto generated file ( Dec 28 2023 18:15:49 ) */

std::vector<std::vector<GToken>> litReservedPreprocessor = {
};

std::vector<std::vector<GToken>> litReservedTable = {
	{
		{ .value = "-", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "uv", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "max", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "sky", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "ray", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "set", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "eta", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "mat", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "Zirr", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "zirr", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "Oidn", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "OIDN", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "oidn", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "path", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "path", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "type", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "flip", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "base", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "name", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "View", .identifier = TOKEN_ID_DATATYPE }, },
	{
		{ .value = "alpha", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "Optix", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "optix", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "OPTIX", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "value", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "sheen", .identifier = TOKEN_ID_OPERATOR },
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
		{ .value = "albedo", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "asym_g", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "medium", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "Medium", .identifier = TOKEN_ID_DATATYPE },
		{ .value = "render", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "window", .identifier = TOKEN_ID_OPERATOR },
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
		{ .value = "Average", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "average", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "density", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "scatter", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "sigma_a", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "sigma_s", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "texture", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "Texture", .identifier = TOKEN_ID_DATATYPE },
		{ .value = "samples", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "indices", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "normals", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "Include", .identifier = TOKEN_ID_DATATYPE },
		{ .value = "include", .identifier = TOKEN_ID_DATATYPE }, },
	{
		{ .value = "Denoiser", .identifier = TOKEN_ID_DATATYPE },
		{ .value = "specular", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "metallic", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "rayleigh", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "emission", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "geometry", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "Material", .identifier = TOKEN_ID_DATATYPE },
		{ .value = "exposure", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "direction", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "luminance", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "wireframe", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "sheenTint", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "clearcoat", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "intensity", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "normalize", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "transform", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "translate", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "subsurface", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "integrator", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "resolution", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "dispersion", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "DirectLight", .identifier = TOKEN_ID_DATATYPE },
		{ .value = "anisotropic", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "reflectance", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "transmission", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "specularTint", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "clearcoatGloss", .identifier = TOKEN_ID_OPERATOR }, },
};

