#pragma once

//#define LEX_DEBUG(...) printf("[DEBUG] "); printf(__VA_ARGS__)
#define LEX_DEBUG(...)
#define LEX_PROCESSOR(name) int name(char **p, size_t n, char **head, size_t *len)

LEX_PROCESSOR(Lex_Number);
LEX_PROCESSOR(Lex_String);
