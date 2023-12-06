/* Build from '/home/felpz/Documents/Cody/lang_tables/lang_python' */
/* Contextual content */
/////////////////////////////////
#include <languages.h>
#include <functional>

// we only capture '@<letter*>.<letter*>...
bool python_decorator_capture(char *head, char *curr){
    char value = *curr;
    // 1 - grab any valid character
    if((value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z'))
        return true;
    // 2 - accept @<something>.<something2> and compoundend strings
    // i.e. : something_something
    return value == '.' || value == '_';
}

TokenizerSupport pythonSupport = {
    .comments = true,
    .strings = true,
    .numbers = true,
    .lookups = true,
    .multilineComment = false,
    .lineCommentChar = '#',
    .procs = { { "@", "a", TOKEN_ID_DATATYPE, 2,  python_decorator_capture }, },
};


/////////////////////////////////
/* Auto generated file ( Dec  6 2023 12:52:52 ) */

std::vector<std::vector<GToken>> pyReservedPreprocessor = {
};

std::vector<std::vector<GToken>> pyReservedTable = {
	{
		{ .value = "+", .identifier = TOKEN_ID_MATH },
		{ .value = "-", .identifier = TOKEN_ID_MATH },
		{ .value = ">", .identifier = TOKEN_ID_MORE },
		{ .value = "<", .identifier = TOKEN_ID_LESS },
		{ .value = "/", .identifier = TOKEN_ID_MATH },
		{ .value = "%", .identifier = TOKEN_ID_MATH },
		{ .value = "!", .identifier = TOKEN_ID_MATH },
		{ .value = "=", .identifier = TOKEN_ID_MATH },
		{ .value = "*", .identifier = TOKEN_ID_ASTERISK },
		{ .value = "&", .identifier = TOKEN_ID_MATH },
		{ .value = "|", .identifier = TOKEN_ID_MATH },
		{ .value = "^", .identifier = TOKEN_ID_MATH },
		{ .value = "~", .identifier = TOKEN_ID_MATH },
		{ .value = ",", .identifier = TOKEN_ID_COMMA },
		{ .value = ".", .identifier = TOKEN_ID_MATH },
		{ .value = "(", .identifier = TOKEN_ID_PARENTHESE_OPEN },
		{ .value = ")", .identifier = TOKEN_ID_PARENTHESE_CLOSE },
		{ .value = "{", .identifier = TOKEN_ID_BRACE_OPEN },
		{ .value = "}", .identifier = TOKEN_ID_BRACE_CLOSE },
		{ .value = "[", .identifier = TOKEN_ID_BRACKET_OPEN },
		{ .value = "]", .identifier = TOKEN_ID_BRACKET_CLOSE },
		{ .value = ":", .identifier = TOKEN_ID_MATH }, },
	{
		{ .value = "if", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "in", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "is", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "as", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "or", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "def", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "try", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "del", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "and", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "for", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "not", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "True", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "None", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "elif", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "else", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "with", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "from", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "pass", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "self", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "raise", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "False", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "while", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "yield", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "break", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "class", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "import", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "return", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "except", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "lambda", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "assert", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "global", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "__len__", .identifier = TOKEN_ID_RESERVED }, },
	{
		{ .value = "finnally", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "nonlocal", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "continue", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "__init__", .identifier = TOKEN_ID_RESERVED },
		{ .value = "__call__", .identifier = TOKEN_ID_RESERVED }, },
	{
		{ .value = "__getitem__", .identifier = TOKEN_ID_RESERVED }, },
};

