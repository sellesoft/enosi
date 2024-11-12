#include "Scanner.h"

#include "ctype.h" 

#include "iro/logger.h"

namespace iro
{

static Logger logger =
  Logger::create("scanner"_str, Logger::Verbosity::Notice);

/* --------------------------------------------------------------------------
 */
b8 Scanner::init(io::IO* in)
{
  this->in = in;
  cache_offset = 0;
  line = column = 1;

  if (!cache.open(chunk_size))
    return ERROR("failed to init cache\n");
  
  readStreamIfNeeded(false, false);
  decodeCurrent();

  return true;
}

/* --------------------------------------------------------------------------
 */
void Scanner::advance(b8 contiguous)
{
  ensureHaveEnoughToDecode();

  if (at('\n'))
  {
    line += 1;
    column = 1;
  }
  else
    column += 1;

  cache_offset += current_codepoint.advance;

  if (!readStreamIfNeeded(contiguous, false))
    return;

  if (!decodeCurrent())
  {
    ERROR("failed to decode at ", line, ":", column, "\n");
    return;
  }
}

/* --------------------------------------------------------------------------
 *  Inefficient peek. Everytime this is called the next codepoint is 
 *  decoded!
 */
u32 Scanner::peek()
{
  readStreamIfNeeded(true, true);
  u64 offset = cache_offset + current_codepoint.advance;
  return utf8::decodeCharacter(
      cache.ptr + offset,
      cache.len - offset);
}

/* --------------------------------------------------------------------------
 */
b8 Scanner::atWhitespace() const
{
  return 0 != isspace(current());
}

/* --------------------------------------------------------------------------
 */
b8 Scanner::isFirstIdentifierChar(u8 c)
{
  return c == '_' || isalpha(c);
}

/* --------------------------------------------------------------------------
 */
b8 Scanner::isIdentifierChar(u8 c)
{
  return isFirstIdentifierChar(c) || isdigit(c);
}

/* --------------------------------------------------------------------------
 */
u64 Scanner::scanIdentifier(io::IO* out)
{
  u64 len = 0;
  if (atFirstIdentifierChar())
  {
    while (atIdentifierChar())
    {
      len += 1;
      scan(out);
    }
  }
  return len;
}

/* --------------------------------------------------------------------------
 */
b8 Scanner::atDigit() const
{
  return 0 != isdigit(current());
}

/* --------------------------------------------------------------------------
 */
String Scanner::scanNumber()
{
  if (atDigit())
  {
    u64 start = cache_offset;
    while (atDigit())
      advance(true);
    return String::from(cache.ptr+start, cache_offset-start);
  }
  else
    return nil;
}

/* --------------------------------------------------------------------------
 *  Scans an identifier and returns a temporary string over it.
 */
String Scanner::scanIdentifier()
{
  if (atFirstIdentifierChar())
  {
    u64 start = cache_offset;
    while (atIdentifierChar())
      advance(true);
    return String::from(cache.ptr+start, cache_offset-start);
  }
  else
    return nil;
}

/* --------------------------------------------------------------------------
 */
u64 Scanner::scanString(io::IO* out)
{
  u64 len = 0;
  if (at('"'))
  {
    advance();
    while (not at('"') and not eof())
    {
      len += 1;
      scan(out);
    }
    if (not eof())
      advance();
  }
  return len;
}

/* --------------------------------------------------------------------------
 *  Scans a string and returns a temporary view over it.
 */
String Scanner::scanString()
{
  if (at('"'))
  {
    advance(true);
    u64 start = cache_offset;
    while (not at('"') and not eof())
      advance(true);
    auto out = String::from(cache.ptr+start, cache_offset-start);
    if (not eof())
      advance(true);
    return out;
  }
  else
    return nil;
}

/* --------------------------------------------------------------------------
 */
b8 Scanner::atComment()
{
  if (at('/'))
  {
    switch (peek())
    {
    case '*':
    case '/': return true;
    }
  }
  return false;
}

/* --------------------------------------------------------------------------
 */
void Scanner::skipComment()
{
  if (not atComment())
    return;
  advance();
  if (at('*'))
  {
    u64 nesting = 0;

    for (;;)
    {
      while (
          not at('*') and
          not at('/') and
          not eof())
        advance();

      if (eof())
        break;

      if (at('*'))
      {
        advance();
        if (at('/'))
        {
          if (nesting == 1)
            break;
          nesting -= 1;
          advance();
        }
      }
      else if (at('/'))
      {
        advance();
        if (at('*'))
        {
          nesting += 1;
          advance();
        }
      }
    }
  }
  else if (at('/'))
  {
    while (not at('\n') and not eof())
      advance();
    if (not eof())
      advance();
  }
}

/* --------------------------------------------------------------------------
 */
void Scanner::scan(io::IO* out)
{
  out->write({cache.ptr+cache_offset,current_codepoint.advance});
  advance();
}

/* --------------------------------------------------------------------------
 */
b8 Scanner::decodeCurrent() 
{
  current_codepoint = 
    utf8::decodeCharacter(
      cache.ptr + cache_offset,
      cache.len - cache_offset);
  return notnil(current_codepoint);
}

/* --------------------------------------------------------------------------
 *  Make sure that we have enough bytes to fully decode the next unicode 
 *  character.
 */
void Scanner::ensureHaveEnoughToDecode()
{
  if (4 > cache.len - (cache_offset + current_codepoint.advance))
  {
    Bytes reserved = cache.reserve(4);
    s64 bytes_read = in->read(reserved);
    cache.commit(bytes_read);
  }
}

/* --------------------------------------------------------------------------
 */
b8 Scanner::readStreamIfNeeded(b8 contiguous, b8 peeking)
{
  if (cache_offset + (peeking? current_codepoint.advance : 0) >= cache.len)
  {
    if (!contiguous)
    {
      cache.clear();
      cache_offset = 0;
    }

    Bytes reserved = cache.reserve(chunk_size);
    s64 bytes_read = in->read(reserved);
    if (bytes_read == -1)
    {
      current_codepoint = nil;
      return ERROR("failed to read more bytes from input stream\n");
    }
    else if (bytes_read == 0)
    {
      at_end = true;
      return false;
    }

    cache.commit(bytes_read);
  }
  return true;
}

}
