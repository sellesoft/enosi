#ifndef _lake_lexer_h
#define _lake_lexer_h

#include "common.h"

#include "generated/token.enum.h" 

struct Lake;

struct Token
{
	tok kind;

	str raw;

	s64 line;
	s64 column;
};

struct Lexer
{
	u8* cursor;

	s64 line;
	s64 column;

	Lake* lake;

	void init(u8* start);

	// null if no more tokens
	Token next_token();
};

#endif
