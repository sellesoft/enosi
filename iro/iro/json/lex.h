/*
 *  Incremental JSON lexer
 *
 */

#ifndef _iro_json_lex_h
#define _iro_json_lex_h

#include "../common.h"
#include "../flags.h"
#include "../unicode.h"
#include "../logger.h"
#include "../io/io.h"
#include "../io/format.h"

#include "csetjmp"

namespace iro::json
{

/* ================================================================================================ json::Token
 */
struct Token
{
  enum class Kind
  {
    Invalid,

    Eof,

    LeftSquare,
    RightSquare,
    LeftBrace,
    RightBrace,
    Colon,
    Comma,

    Whitespace,

    True,
    False,
    Null,
    Number,
    String,
  };

  Kind kind = Kind::Invalid;

  s32 loc = 0;
  s32 len = 0;
};

}

DefineNilValue(iro::json::Token, {}, 
  { return x.kind == iro::json::Token::Kind::Invalid; });

namespace iro::json
{

/* ================================================================================================ json::Lexer
 */
struct Lexer
{
  str stream_name = nil;

  io::Memory cache = nil;
  io::IO* in = nullptr;

  utf8::Codepoint current_codepoint = nil;
  u64 current_offset = 0;

  b8 at_end = false;

  jmp_buf* failjmp = nullptr;

  b8 init(io::IO* input_stream, str stream_name, jmp_buf* failjmp);
  void deinit();

  Token nextToken();

  str getRaw(Token t) { return { cache.buffer + t.loc, u64(t.len) }; }

private:

  Token curt = nil;

  b8 readStreamIfNeeded();
  b8 decodeCurrent();

  void initCurt();
  void finishCurt(Token::Kind kind, s32 len_offset = 0);

  u32 current();

  b8 at(u8 c);
  b8 eof();
  b8 atFirstIdentifierChar();
  b8 atIdentifierChar();
  b8 atDigit();
  b8 atWhitespace();

  void advance(s32 n = 1);
  void skipWhitespace();

  template<io::Formattable... T>
  Token errorHere(T... args);

  template<io::Formattable... T>
  Token errorAt(s32 offset, T... args);
};

}

#endif // _iro_json_lex_h
