#include "lexer.h"
#include "lake.h"

#include "ctype.h"
#include "stdlib.h"

static Logger logger = Logger::create("lake.lexer"_str, Logger::Verbosity::Warn);

template<typename... T>
b8 Lexer::errorHere(T... args)
{
	ERROR(sourcename, ":", line, ":", column, ": ", args..., "\n");
	return false;
}


b8 Lexer::init(str sourcename_, io::IO* instream)
{
	in = instream;
	sourcename = sourcename_;
	cache.open();
	cursor_codepoint = nil;
	advance();
	return true;
}

void Lexer::deinit()
{
	in = nullptr;
}	

u32 Lexer::current() { return cursor_codepoint; }
u8* Lexer::currentptr() { return cursor.bytes - cursor_codepoint.advance; }

b8 Lexer::eof() { return current() == 0; }
b8 Lexer::at(u32 c) { return current() == c; }

b8 Lexer::atFirstIdentifierChar() { return isalpha(current()) || at('_'); }
b8 Lexer::atIdentifierChar() { return atFirstIdentifierChar() || isdigit(current()); }

b8 Lexer::atWhitespace() { return isspace(current()) != 0; }

void Lexer::advance(s32 n)
{
	auto readStreamIfNeeded = [this]()
	{
		if (!cursor.isEmpty() &&
			!cache.atEnd())
			return;

		Bytes reserved = cache.reserve(512);
		s64 bytes_read = in->read(reserved);
		if (bytes_read == -1)
		{
			// TODO(sushi) if lake is ever changed to be usable as a library
			//             make this not fatal and use longjmp instead
			errorHere("failed to read more bytes from input stream!");
			exit(1);
		}
		else if (bytes_read == 0)
		{
			// for some reason i have to do this for this lexer
			// but not lpp's lexer and its kiiinda scary!!
			cursor_codepoint.codepoint = 0;
			cursor_codepoint.advance = 1;
		}
		cache.commit(bytes_read);
		cursor.bytes = reserved.ptr;
		cursor.len = bytes_read;
	};

	for (s32 i = 0; i < n; i++)
	{
		readStreamIfNeeded();
		if (cursor_codepoint.codepoint == 0)
			return;

		cursor_codepoint = cursor.advance();

		if (at('\n'))
		{
			line += 1;
			column = 1;
		}
		else
			column += 1;
	}

}

#include "generated/token.kwmap.h"

Token Lexer::nextToken()
{ 
	curt.kind = tok::Eof;
	curt.offset = currentptr() - cache.buffer;

	s32 len_offset = 0;

	// defer { INFO("outputting token ", tok_strings[(u32)curt.kind], ": ", getRaw(curt), "\n"); };

	if (atWhitespace())
	{
		curt.kind = tok::Whitespace;
		while(atWhitespace()) 
			advance();
	}
	else switch (current())
	{
		case 0: {
			curt.len = 0;
			return curt;
		} break;

#define one_char_token(c, k)     \
		case c: {                \
			curt.kind = tok::k;  \
			advance();           \
		} break;

		one_char_token(';', Semicolon);
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
		one_char_token('`', Backtick);
		one_char_token('$', Dollar);
		one_char_token('/', Solidus);
		one_char_token('#', Pound);
		
#define one_or_two_char_token(c0, k0, c1, k1) \
		case c0: {                            \
			curt.kind = tok::k0;              \
			advance();                        \
			if (current() == c1)              \
			{                                 \
				curt.kind = tok::k1;          \
				advance();                    \
			}                                 \
		} break;

		one_or_two_char_token('<', LessThan,    '=', LessThanOrEqual);
		one_or_two_char_token('>', GreaterThan, '=', GreaterThanOrEqual);
		one_or_two_char_token(':', Colon,       '=', ColonEqual);
		one_or_two_char_token('=', Equal,       '=', EqualDouble);

		case '~': {
			advance();
			if (current() != '=')
			{
				errorHere("unknown token. only valid glyph after '~' is '='.");
				exit(1);
			}
			advance();
			curt.kind = tok::TildeEqual;
		} break;

		case '?': {
			advance();
			if (current() != '=')
			{
				errorHere("unknown token. only valid glyph after '?' is '='.");
				exit(1);
			}
			advance();
			curt.kind = tok::QuestionMarkEqual;
		} break;

		case '.': {
			curt.kind = tok::Dot;
			advance();
			if (current() == '.')
			{
				curt.kind = tok::DotDouble;
				advance();
				if (current() == '.')
				{
					curt.kind = tok::Ellipses;
					advance();
				}
				else if (current() == '=')
				{
					curt.kind = tok::DotDoubleEqual;
					advance();
				}
			}
		} break;

		case '"': {
			u64 l = line, c = column;
			curt.kind = tok::String;
			curt.offset += 1;
			len_offset = -1; // haha 
			advance();
			while (!at('"') && !eof()) 
				advance();

			if (eof())
			{
				errorHere("encountered eof while consuming string that began at ", l, ":", c);
				exit(1);
			}
			advance();
		} break;

		case '-': {
			curt.kind = tok::Minus;
			advance();
			if (current() == '-')
			{
				// consume comment and return the following token
				while (!at('\n') && !eof()) advance();
				return nextToken();
			}
		} break;

		default: {
			if (isdigit(current()))
			{
				while (isdigit(current())) advance();

				switch (current())
				{
					case 'e':
					case 'E':
					case '.': {
						advance();
						while(isdigit(current())) advance();
						curt.len = (currentptr() - cache.buffer) - curt.offset;
					} break;

					case 'x':
					case 'X':{
						advance();
						while(isdigit(current())) advance(); 
						curt.kind = tok::Number;
						curt.len = (currentptr() - cache.buffer) - curt.offset;
					} break;

					default: {
						curt.len = (currentptr() - cache.buffer) - curt.offset;
						curt.kind = tok::Number;
					} break;
				}
			}
			else
			{
				if (eof())
				{
					curt.len = 0;
					return curt;
				}

				// must be either a keyword or identifier
				if (!atFirstIdentifierChar())
				{
					errorHere("invalid token: ", (char)current());
					exit(1);
				}

				advance();
				while (atIdentifierChar() && !eof()) 
					advance();

				curt.len = (currentptr() - cache.buffer) - curt.offset + len_offset;
				curt.kind = is_keyword_or_identifier(getRaw(curt));
				return curt;
			}
		} break;
	}

	curt.len = (currentptr() - cache.buffer) - curt.offset + len_offset;

	return curt;
}
