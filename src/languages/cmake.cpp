/* Build from '/home/felipe/Documents/Cody/lang_tables/lang_cmake' */
/* Contextual content */
/////////////////////////////////
#include <languages.h>

TokenizerSupport cmakeSupport = {
    .comments = true,
    .strings = true,
    .numbers = true,
    .lookups = true,
    .multilineComment = false,
    .lineCommentChar = '#',
};


/////////////////////////////////
/* Auto generated file ( Aug 31 2022 16:29:34 ) */

std::vector<std::vector<GToken>> cmakeReservedPreprocessor = {
};

std::vector<std::vector<GToken>> cmakeReservedTable = {
	{
		{ .value = "$", .identifier = TOKEN_ID_DATATYPE },
		{ .value = "{", .identifier = TOKEN_ID_DATATYPE },
		{ .value = "}", .identifier = TOKEN_ID_DATATYPE }, },
	{
		{ .value = "if", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "NOT", .identifier = TOKEN_ID_PREPROCESSOR },
		{ .value = "set", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "GLOB", .identifier = TOKEN_ID_PREPROCESSOR },
		{ .value = "list", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "file", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "else", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "EQUAL", .identifier = TOKEN_ID_PREPROCESSOR },
		{ .value = "endif", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "elseif", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "export", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "option", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "COMMAND", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "MATCHES", .identifier = TOKEN_ID_PREPROCESSOR },
		{ .value = "install", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "project", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "message", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "STREQUAL", .identifier = TOKEN_ID_PREPROCESSOR },
		{ .value = "function", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "find_path", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "add_library", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "endfunction", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "find_package", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "find_library", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "source_group", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "find_package", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "get_property", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "set_property", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "CMAKE_COMMAND", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "enable_testing", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "add_executable", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "configure_file", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "add_definitions", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "execute_process", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "link_directories", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "add_subdirectory", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "CMAKE_BUILD_TYPE", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "mark_as_advanced", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "CMAKE_SOURCE_DIR", .identifier = TOKEN_ID_DATATYPE },
		{ .value = "CMAKE_BINARY_DIR", .identifier = TOKEN_ID_DATATYPE }, },
	{
		{ .value = "add_custom_target", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "CMAKE_CXX_COMPILER", .identifier = TOKEN_ID_DATATYPE },
		{ .value = "add_custom_command", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "include_directories", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "target_link_libraries", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "set_target_properties", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "target_compile_options", .identifier = TOKEN_ID_OPERATOR },
		{ .value = "cmake_minimum_required", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "CMAKE_CURRENT_BINARY_DIR", .identifier = TOKEN_ID_DATATYPE },
		{ .value = "CMAKE_CURRENT_SOURCE_DIR", .identifier = TOKEN_ID_DATATYPE }, },
	{
		{ .value = "target_include_directories", .identifier = TOKEN_ID_OPERATOR }, },
	{
		{ .value = "CMAKE_RUNTIME_OUTPUT_DIRECTORY", .identifier = TOKEN_ID_DATATYPE }, },
};

