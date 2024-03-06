#include "lexer.h"
#include "lake.h"

#include "ctype.h"
#include "stdlib.h"

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

/* ------------------------------------------------------------------------------------------------
 *  Helpers for querying what the character is at the current position in the stream.
 */
static b8 eof(Lexer* l) { return current(l) == 0; }
static b8 at(Lexer* l, u8 c) { return current(l) == c; }
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

#include "generated/token.kwmap.h"

Token Lexer::next_token()
{
	Token t = {}; 
	
	t.kind   = tok::Eof;
	t.line   = line;
	t.column = column;
	t.raw.s  = cursor;

	s32 len_offset = 0;

	if (isspace(current(this)))
	{
		t.kind = tok::Whitespace;
		while(isspace(current(this))) advance(this);
	}
	else switch (current(this))
	{
		case 0: {
			t.raw.len = 0;
			return t;
		} break;

#define one_char_token(c, k) \
		case c: {            \
			t.kind = tok::k; \
			cursor += 1;     \
		} break;

		one_char_token(';', Semicolon);
		one_char_token('=', Equal);
		one_char_token(',', Comma);
		one_char_token('(', ParenLeft);
		one_char_token(')', ParenRight);
		one_char_token('{', BraceLeft);
		one_char_token('}', BraceRight);
		one_char_token('[', SquareLeft);
		one_char_token(']', SquareRight);
		one_char_token('*', Asterisk);
		one_char_token('^', Caret);
		one_char_token('%', Percent);
		one_char_token('+', Plus);
		one_char_token('-', Minus);
		one_char_token('`', Backtick);
		one_char_token('$', Dollar);
		one_char_token('/', Solidus)
		
#define one_or_two_char_token(c0, k0, c1, k1) \
		case c0: {                            \
			t.kind = tok::k0;                 \
			advance(this);                    \
			if (current(this) == c1)          \
			{                                 \
				t.kind = tok::k1;             \
				advance(this);                \
			}                                 \
		} break;

		one_or_two_char_token('<', LessThan,    '=', LessThanOrEqual);
		one_or_two_char_token('>', GreaterThan, '=', GreaterThanOrEqual);
		one_or_two_char_token(':', Colon,       '=', ColonEqual);
		
		case '~': {
			advance(this);
			if (current(this) != '=')
			{
				error(lake->path, line, column, "unknown token. only valid glyph after '~' is '='.");
				exit(1);
			}
			advance(this);
			t.kind = tok::TildeEqual;
		} break;

		case '?': {
			advance(this);
			if (current(this) != '=')
			{
				error(lake->path, line, column, "unknown token. only valid glyph after '?' is '='.");
				exit(1);
			}
			advance(this);
			t.kind = tok::QuestionMarkEqual;
		} break;

		case '.': {
			t.kind = tok::Dot;
			advance(this);
			if (current(this) == '.')
			{
				t.kind = tok::DotDouble;
				advance(this);
				if (current(this) == '.')
				{
					t.kind = tok::Ellises;
					advance(this);
				}
			}
		} break;

		case '"': {
			u64 l = line, c = column;
			t.kind = tok::String;
			t.raw.s += 1;
			len_offset = -1; // haha 
			advance(this);
			while (current(this) != '"' && !eof(this)) advance(this);

			if (eof(this))
			{
				error(lake->path, line, column, "encountered eof while consuming string that began at ", l, ":", c);
				exit(1);
			}
			advance(this);
		} break;

		default: {
			if (isdigit(current(this)))
			{
				while (isdigit(current(this))) advance(this);

				switch (current(this))
				{
					case 'e':
					case 'E':
					case '.': {
						advance(this);
						while(isdigit(current(this))) advance(this);
						t.raw.len = cursor - t.raw.s;
					} break;

					case 'x':
					case 'X':{
						advance(this);
						while(isdigit(current(this))) advance(this);
						t.kind = tok::Number;
						t.raw.len = cursor - t.raw.s;
					} break;

					default: {
						t.raw.len = cursor - t.raw.s;
						t.kind = tok::Number;
					} break;
				}
			}
			else
			{
				// must be either a keyword or identifier
				if (!isalpha(current(this)))
				{
					error(lake->path, line, column, "invalid token: ", (char)current(this));
					exit(1);
				}

				advance(this);
				while (at_identifier_char(this) && !eof(this)) advance(this);

				t.kind = is_keyword_or_identifier(t.raw);
			}
		} break;
	}

	t.raw.len = (cursor - t.raw.s) + len_offset;

	return t;
}
