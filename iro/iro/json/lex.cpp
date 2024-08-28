#include "lex.h"
#include "../logger.h"
#include "../linemap.h"

#include "ctype.h"

namespace iro::json
{

static auto logger = 
  Logger::create("json.lexer"_str, Logger::Verbosity::Notice);

/* ----------------------------------------------------------------------------
 */
u32 Lexer::current() { return current_codepoint.codepoint; }

b8 Lexer::at(u8 c)      { return current() == c; }
b8 Lexer::eof()         { return at_end || at(0); }

b8 Lexer::atFirstIdentifierChar() { return isalpha(current()) || at('_'); }
b8 Lexer::atIdentifierChar() { return atFirstIdentifierChar() || 
                                      isdigit(current()); }
b8 Lexer::atWhitespace() { return isspace(current()) != 0; }

/* ----------------------------------------------------------------------------
 *  Returns false if we've reached the end of the buffer.
 */
b8 Lexer::readStreamIfNeeded()
{
  if (current_offset < cache.len)
    return true;

  TRACE("reading more bytes from stream... \n");

  auto reserved = cache.reserve(16);
  if (reserved.len == 0)
  {
    ERROR("failed to reserve space in stream cache\n");
    return false;
  }
  auto bytes_read = in->read(reserved);
  TRACE(bytes_read, " bytes read... \n");
  if (bytes_read == -1)
  {
    current_codepoint = nil;
    errorHere("failed to read more bytes from input stream");
    longjmp(*failjmp, 0);
  }
  else if (bytes_read == 0) // || reserved.end()[-1] == 0)
  {
    TRACE("0 bytes read, must be the end of the file...\n");
    at_end = true;
    current_codepoint = nil;
    return false;
  }
  cache.commit(bytes_read);
  TRACE("chunk is: \n", cache.asStr().sub(current_offset), "\n");

  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Lexer::decodeCurrent()
{
  current_codepoint = 
    utf8::decodeCharacter(
      cache.buffer + current_offset, 
      cache.len - current_offset);

  return notnil(current_codepoint);
}

/* ----------------------------------------------------------------------------
 */
void Lexer::advance(s32 n)
{
  for (s32 i = 0; i < n; ++i)
  {
    current_offset += current_codepoint.advance;

    if (!readStreamIfNeeded())
      return;

    if (!decodeCurrent())
    {
      errorHere("encountered invalid codepoint!");
      longjmp(*failjmp, 0);
    }
  }
}

/* ----------------------------------------------------------------------------
 */
template<io::Formattable... T>
Token Lexer::errorHere(T... args)
{
  auto result = cache.asStr().findLineAndColumnAtOffset(cache.len);
  ERROR(
    stream_name, ":", result.line, ":", result.column, ": ", args..., "\n");
  return nil;
}

/* ----------------------------------------------------------------------------
 */
template<io::Formattable... T>
Token Lexer::errorAt(s32 offset, T... args)
{
  auto result = cache.asStr().findLineAndColumnAtOffset(offset);
  ERROR(
    stream_name, ":", result.line, ":", result.column, ": ", args..., "\n");
  return nil;
}

/* ----------------------------------------------------------------------------
 */
void Lexer::skipWhitespace()
{
  while (atWhitespace())
    advance();
}

/* ----------------------------------------------------------------------------
 */
b8 Lexer::init(io::IO* input_stream, str stream_name, jmp_buf* failjmp)
{
  TRACE("initializing with input stream ", (void*)input_stream);

  in = input_stream;
  this->stream_name = stream_name;
  this->failjmp = failjmp;

  at_end = false;

  if (!cache.open())
  {
    ERROR("failed to open cache\n");
    return false;
  }

  readStreamIfNeeded();
  decodeCurrent();

  return true;
}

/* ------------------------------------------------------------------------------------------------
 */
void Lexer::deinit()
{
  cache.close();
  *this = {};
}

/* ----------------------------------------------------------------------------
 */
void Lexer::initCurt()
{
  curt.kind = Token::Kind::Eof;
  curt.loc = current_offset;
}

/* ----------------------------------------------------------------------------
 */
void Lexer::finishCurt(Token::Kind kind, s32 len_offset)
{
  s32 len = len_offset + current_offset - curt.loc;
  curt.kind = kind;
  curt.len  = len;
  TRACE("finished ", getRaw(curt).sub(0, curt.len > 16 ? 16 : curt.len), "\n");
}

/* ------------------------------------------------------------------------------------------------
 */
Token Lexer::nextToken()
{
  using enum Token::Kind;

  skipWhitespace();

  if (eof())
    return {Eof};

  initCurt();

  switch (current())
  {
#define singleglyph(c, k) case c: advance(); finishCurt(k); break;
  singleglyph('[', LeftSquare);
  singleglyph(']', RightSquare);
  singleglyph('{', LeftBrace);
  singleglyph('}', RightBrace);
  singleglyph(':', Colon);
  singleglyph(',', Comma);
#undef singleglyph

  case '"':
    // just consume the raw string here, it is processed later in parsing
    advance();
    initCurt();
    for (;;)
    {
      // skip anything after an escape 
      if (at('\\'))
      {
        advance(2);
        continue;
      }

      if (at('"'))
        break;
      if (eof())
        return errorAt(curt.loc, 
            "unexpected end of file while consuming string");
      advance();
    }
    finishCurt(String);
    advance();
    break;

  default: 
    {
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
            errorHere(
                "expected at least one more digit after frac decimal "
                "point");
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
            errorHere("expected at least one digit for number exponent");
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
          return errorHere("expected a digit after '-'");

        if (!consumeNumber())
          return nil;

        finishCurt(Number);
      }
      else if (isdigit(current()))
      {
        if (!consumeNumber())
          return nil;

        finishCurt(Number);
      }
      else if (isalpha(current()))
      {
        while (isalpha(current()))
          advance();

        str raw = {cache.buffer + curt.loc, current_offset - curt.loc };
        switch (raw.hash())
        {
        case "true"_hashed: 
          finishCurt(True);
          break;
        case "false"_hashed:
          finishCurt(False);
          break;
        case "null"_hashed:
          finishCurt(Null);
          break;
        default:
          return errorHere("unrecognized literal name '", raw, "'");
        }
      }
    } 
    break;
  }

  return curt;
}

}
