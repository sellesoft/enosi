/*
 *  JSON parser
 *
 */

#ifndef _iro_json_parser_h
#define _iro_json_parser_h

#include "../unicode.h"

#include "lex.h"
#include "types.h"

namespace iro::json
{

/* ============================================================================
 */
struct Parser
{
  using TKind = Token::Kind;
  using VKind = Value::Kind;

  typedef SList<Value> ValueStack;

  Lexer lexer;
  JSON* json;
  Token curt;

  ValueStack value_stack;

  io::IO* in;

  jmp_buf* failjmp;


  /* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
   */


  // Initializes the parser with the stream to parse
  // and a JSON object to fill with information.
  // 'stream_name' is used in error reporting
  b8 init(
      io::IO*  input_stream, 
      JSON*    json, 
      str      stream_name, 
      jmp_buf* failjmp = nullptr);
  void deinit();

  b8 start();

private:

  template<typename... T>
  b8 errorHere(T... args);

  template<typename... T>
  b8 errorAt(s64 line, s64 column, T... args);

  template<typename... T>
  b8 errorNoLocation(T... args);

  void nextToken();

  b8 at(TKind kind);

  b8 value();
  b8 object();
  b8 array();
};

} // namespace json

#endif // _iro_json_parser_h
