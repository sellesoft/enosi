#include "lexer.h"

#include "ctype.h"

void Lexer::init(u8* start)
{
	cursor = start;
	line = column = 1;
}

/* ------------------------------------------------------------------------------------------------
 *  Helpers for getting a character relative to the current position in the stream.
 */
static u8 current(Lexer* l) { return *l->cursor; }
static u8 next(Lexer* l) { return *(l->cursor + 1); }
static u8 nextnext(Lexer* l) { return *(l->cursor + 2); }


/* ------------------------------------------------------------------------------------------------
 *  Helpers for querying what the character is at the current position in the stream.
 */
static b8 eof(Lexer* l) { return current(l) == 0; }
static b8 at(Lexer* l, u8 c) { return current(l) == c; }
static b8 next_at(Lexer* l, u8 c) { return next(l) == c; }
static b8 at_identifier_char(Lexer* l) { return isalnum(current(l)) || current(l) == '_'; }

static void advance(Lexer* l)
{
	if (at(l, '\n'))
	{
		l->line += 1;
		l->column = 1;
	}
	else
		l->column += 1;
	l->cursor += 1;
}

Token Lexer::next_token()
{
	Token t = {}; 
	
	t.kind   = tok::NULL;
	t.line   = line;
	t.column = column;
	t.raw.s  = cursor;

	switch (*cursor)
	{
#define one_char_token(c, k) \
		case c: {            \
			t.kind = tok::k; \
			cursor += 1;     \
		} break;

		one_char_token(':', Colon);
		one_char_token(';', Semicolon);
		one_char_token('=', Equal);
		one_char_token(',', Comma);
		one_char_token('(', ParenLeft);
		one_char_token(')', ParenRight);
		one_char_token('{', BraceLeft);
		one_char_token('}', BraceRight);
		one_char_token('[', SquareLeft);
		one_char_token(']', SquareRight);
		one_char_token('.', Dot);
		one_char_token('*', Asterisk);
		one_char_token('^', Caret);
		one_char_token('%', Percent);
		one_char_token('+', Plus);
		one_char_token('-', Minus);
		


	}

	return t;
}
