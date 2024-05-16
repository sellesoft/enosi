#ifndef _lake_lexer_h
#define _lake_lexer_h

#include "iro/common.h"
#include "iro/unicode.h"
#include "iro/io/io.h"
#include "iro/logger.h"
#include "iro/linemap.h"

#include "generated/token.enum.h" 

using namespace iro;

#include "generated/token.strings.h"

struct Lake;

struct Token
{
	tok kind;

	u32 offset;
	u32 len;

	u32 line;
	u32 column;
};

DefineNilValue(Token, {tok::Nil}, { return x.kind == tok::Nil; });

struct Lexer
{
	str sourcename;
	io::IO* in;
	
	u32 line;
	u32 column;

	b8   init(str sourcename, io::IO* in);
	void deinit();

	// null if no more tokens
	Token nextToken();

	str getRaw(Token t) { return {cache.buffer + t.offset, t.len}; }

private:

	Token curt;

	void initCurt();
	void finishCurt(tok kind, s32 len_offset = 0);

	utf8::Codepoint cursor_codepoint;
	str cursor;

	// cached buffer so we can get raw strings from tokens
	io::Memory cache;

	u32 current();
	u8* currentptr();

	b8 at(u32 c);
	b8 eof();
	b8 atFirstIdentifierChar();
	b8 atIdentifierChar();
	b8 atWhitespace();

	void advance(s32 n = 1);
	void skipWhitespace();

	template<typename... T>
	b8 errorHere(T... args);

};

#endif
