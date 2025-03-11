/*
 *  Parser for lpp files. Takes tokens and transforms them into a metaprogram.
 */

#ifndef _lpp_Parser_h
#define _lpp_Parser_h

#include "iro/Common.h"
#include "iro/containers/Array.h"

#include "Lex.h"
#include "csetjmp"

namespace lpp
{

/* ============================================================================
 */
struct Parser
{
  Lexer lexer;

  Token* curt;

  Source* source;

  io::IO* in;
  io::IO* out;

  // A mapping from the generated metacode back to the original input.
  // This is used to generate a line mapping later so that lua errors can
  // be properly mapped to the lpp file.
  struct LocMapping 
  { 
    Token token;
    s32 from; 
    s32 to; 
  };
  Array<LocMapping> locmap;
  s32 bytes_written = 0; // tracked solely for the locmap

  jmp_buf err_handler;

  b8 init(
    Source* source, 
    io::IO* instream, 
    io::IO* outstream,
    LexerConsumer* lex_diag_consumer);

  void deinit();

  b8 run();

  // internal stuff

  void pushLocMap(s32 from_offset = 0, s32 to_offset = 0);

  b8 nextToken();
  b8 nextSignificantToken();

  b8 at(Token::Kind kind);

  String getRaw();

  u64 curtIdx() const { return curt - lexer.tokens.arr; }

  void writeTokenSanitized();

  template<typename... T>
  void writeOut(T... args);
};

}

#endif // _lpp_parser_h
