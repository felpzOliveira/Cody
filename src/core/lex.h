#pragma once
//I like LEX = logical executor :)

#define LEX_PROCESSOR(name) int name(char **p, size_t n, char **head, size_t *len)

/* Define finite automata for a few entities...*/
LEX_PROCESSOR(Lex_Number);
LEX_PROCESSOR(Lex_String);

/* Generic utility for parsing, slow, do not use for performance critical stuff */
#define LEX_PROC_CALLBACK(name) int name(char **p, unsigned int size)
typedef LEX_PROC_CALLBACK(Lex_LineProcessorCallback);

/*
* Parses a file line-by-line giving each line to a processor callback.
*/
void Lex_LineProcess(const char *path, Lex_LineProcessorCallback *processor);