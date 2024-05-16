#include "lex.h"
#include "../logger.h"

#include "ctype.h"

namespace iro::json
{


/* ================================================================================================= 
 * =================================================================================== Lexer helpers
 */
u32 Lexer::current()     { return cursor_codepoint; }

b8 Lexer::at(u8 c)      { return current() == c; }
b8 Lexer::eof()         { return at(0); }

b8 Lexer::atFirstIdentifierChar() { return isalpha(current()) || at('_'); }
b8 Lexer::atIdentifierChar() { return atFirstIdentifierChar() || isdigit(current()); }
b8 Lexer::atWhitespace() { return isspace(current()) != 0; }

void Lexer::advance(s32 n)
{
    auto readStreamIfNeeded = [this]()
    {
        if (stream_buffer.atEnd() ||
            cursor.isEmpty())
        {
            Bytes bytes = stream_buffer.reserve(128);
            s64 bytes_read = in->read(bytes);
            if (!bytes_read)
            {
                cursor_codepoint = nil;
                errorHere("failed to read more bytes from input stream");
                return;
            }
            stream_buffer.commit(bytes_read);
            cursor = str::from(bytes.ptr, bytes.len);
        }
    };

    readStreamIfNeeded();

	for (s32 i = 0; i < n; i++)
	{
		if (at('\n'))
		{
			line  += 1;
			column = 1;
		}
		else
		{
			column += 1;
		}

        readStreamIfNeeded();
		cursor_codepoint = cursor.advance();

        if (!cursor_codepoint)
        {
            errorHere("encountered invalid codepoint!");
            return;
        }
	}
}

void Lexer::skipWhitespace()
{
	while (atWhitespace())
		advance();
}

/* ------------------------------------------------------------------------------------------------ json::Lexer::init
 */
b8 Lexer::init(io::IO* input_stream, utf8::str stream_name_, Logger::Verbosity v)
{
    logger.init("json.lexer"_str, v);

    INFO("initializing with input stream ", (void*)input_stream);

    in = input_stream;
	flags = Flags::none();
	stream_name = stream_name_;
	line   = 1;
	column = 0;

    stream_buffer.open();
	advance();

	return true;
}

/* ------------------------------------------------------------------------------------------------ json::Lexer::init
 */
void Lexer::deinit()
{
	cursor = str{};
	flags = Flags::none();
}

/* ------------------------------------------------------------------------------------------------ json::Lexer::nextToken
 */
Token Lexer::nextToken()
{
	using enum Token::Kind;

	Token t = {};

	auto resetToken = [&t, this]()
	{
		t.line = line;
		t.column = column;
		t.raw.bytes = cursor.bytes - 1;
	};

	auto finishToken = [&t, this](Token::Kind kind)
	{
		t.raw.len = cursor.bytes - 1 - t.raw.bytes;
		t.kind = kind;
	};

	if (atWhitespace())
	{
		skipWhitespace();
		if (flags.test(Flag::ReturnWhitespaceTokens))
		{
			t.raw.len = cursor.bytes - t.raw.bytes;
			t.kind = Whitespace;
			return t;
		}
	}

	if (eof() || cursor.isEmpty())
		return {Eof};

	resetToken();

	switch (current())
	{
#define singleglyph(c, k) case c: advance(); finishToken(k); break;
		singleglyph('[', LeftSquare);
		singleglyph(']', RightSquare);
		singleglyph('{', LeftBrace);
		singleglyph('}', RightBrace);
		singleglyph(':', Colon);
		singleglyph(',', Comma);

		case '"': {
			// just consume the raw string here, it is processed later in parsing
			advance();
			resetToken();
			for (;;)
			{
				// skip anything after an escape 
				if (at('\\'))
					advance(2);
				if (at('"'))
					break;
				if (eof())
					return errorAt(t.line, t.column, "unexpected end of file while consuming string\n");
				advance();

			}
			finishToken(String);
			advance();
		} break;

		default: {

			auto consumeNumber = [this]()
			{
				if (at('0'))
					advance();
				else while (isdigit(current()))
					advance();

				if (at('.'))
				{
					advance();
					if (!isdigit(current()))
					{
						errorHere("expected at least one more digit after frac decimal point\n");
						return false;
					}
					while (isdigit(current()))
						advance();
				}

				if (at('e') || at('E'))
				{
					advance();
					if (at('+') || at('-'))
						advance();
					if (!isdigit(current()))
					{
						errorHere("expected at least one digit for number exponent\n");
						return false;
					}
					while (isdigit(current()))
						advance();
				}

				return true;
			};

			if (current() == '-')
			{
				advance();
				if (!isdigit(current()))
					return errorHere("expected a digit after '-'\n");

				if (!consumeNumber())
					return Token::invalid();

				finishToken(Number);
			}
			else if (isdigit(current()))
			{
				if (!consumeNumber())
					return Token::invalid();

				finishToken(Number);
			}
			else if (isalpha(current()))
			{
				while (isalpha(current()))
					advance();
				
				u64 hash = t.raw.hash();
				switch (hash)
				{
				case "true"_hashed: 
					finishToken(True);
					break;
				case "false"_hashed:
					finishToken(False);
					break;
				case "null"_hashed:
					finishToken(Null);
					break;
				default:
					return errorHere("unrecognized literal name '", t.raw, "'\n");
				}
			}
		} break;
	}

	return t;
}

}
