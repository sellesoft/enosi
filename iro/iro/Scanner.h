/*
 *  A helper wrapper around an IO for scanning common lexical patterns.
 */

#ifndef _iro_scanner_h
#define _iro_scanner_h

#include "io/io.h"
#include "logger.h"

namespace iro
{

/* ============================================================================
 */
struct Scanner
{
  static const u64 chunk_size = 64;

  io::Memory cache = {};

  io::IO* in = nullptr;

  u64 line = 1, column = 1;

  // Current offset into the cached chunk.
  // NOTE(sushi) this is a bit dumb, since io::Memory stores its own read 
  //             offset. I really need to add something else that is just 
  //             for writing to.
  u64 cache_offset = 0;

  utf8::Codepoint current_codepoint = nil;

  b8 at_end = false;

  /* --------------------------------------------------------------------------
   */
  b8 init(io::IO* in);

  /* --------------------------------------------------------------------------
   */
  void deinit()
  {
    cache.close();
  }

  /* --------------------------------------------------------------------------
   */
  u32 current() const { return current_codepoint.codepoint; }

  /* --------------------------------------------------------------------------
   */
  b8 at(u32 c) const { return current() == c; }

  /* --------------------------------------------------------------------------
   */
  b8 eof() const { return at_end || at(0) || isnil(current_codepoint); }


  /* --------------------------------------------------------------------------
   */
  void advance(b8 contiguous = false);

  /* --------------------------------------------------------------------------
   *  Inefficient peek. Everytime this is called the next codepoint is 
   *  decoded!
   */
  u32 peek();

  /* --------------------------------------------------------------------------
   */
  b8 atWhitespace() const;

  /* --------------------------------------------------------------------------
   */
  void skipWhitespace() 
  {
    while (atWhitespace() and not eof())
      advance();
  }

  /* --------------------------------------------------------------------------
   */
  void skipWhitespaceAndComments()
  {
    for (;;)
    {
      skipWhitespace();
      if (not atComment())
        break;
      skipComment();
    }
  }

  /* --------------------------------------------------------------------------
   */
  static b8 isFirstIdentifierChar(u8 c);
  static b8 isIdentifierChar(u8 c);

  /* --------------------------------------------------------------------------
   */
  b8 atFirstIdentifierChar() const { return isFirstIdentifierChar(current()); }
  b8 atIdentifierChar() const { return isIdentifierChar(current()); }

  /* --------------------------------------------------------------------------
   */
  u64 scanIdentifier(io::IO* out);

  /* --------------------------------------------------------------------------
   *  Scans an identifier and returns a temporary string over it.
   */
  String scanIdentifier();

  /* --------------------------------------------------------------------------
   */
  b8 atDigit() const;

  /* --------------------------------------------------------------------------
   */
  String scanNumber();

  /* --------------------------------------------------------------------------
   */
  u64 scanString(io::IO* out);

  /* --------------------------------------------------------------------------
   *  Scans a string and returns a temporary view over it.
   */
  String scanString();

  /* --------------------------------------------------------------------------
   */
  b8 atComment();

  /* --------------------------------------------------------------------------
   */
  void skipComment();

private:
  // Internal stuff not really relevant to the API.

  /* --------------------------------------------------------------------------
   */
  void scan(io::IO* out);

  /* --------------------------------------------------------------------------
   */
  b8 decodeCurrent();

  /* --------------------------------------------------------------------------
   *  Make sure that we have enough bytes to fully decode the next unicode 
   *  character.
   */
  void ensureHaveEnoughToDecode();

  /* --------------------------------------------------------------------------
   */
  b8 readStreamIfNeeded(b8 contiguous, b8 peeking);
}; 

}

#endif
