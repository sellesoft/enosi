#include "Parser.h"
#include "Lex.h"
#include "iro/LineMap.h"
#include "iro/fs/File.h"

#include "ctype.h"

namespace lpp
{

static Logger logger = 
  Logger::create("lpp.parser"_str, Logger::Verbosity::Notice);

/* ----------------------------------------------------------------------------
 */
b8 Parser::init(
    Source* src,
    io::IO* instream, 
    io::IO* outstream,
    LexerConsumer* lex_diag_consumer)
{
  assert(instream && outstream);

  TRACE("initializing on stream '", src->name, "'\n");

  locmap = Array<LocMapping>::create();
  in = instream;
  out = outstream;
  source = src;

  if (!lexer.init(in, src, lex_diag_consumer))
    return false;

  if (setjmp(lexer.err_handler))
  {
    lexer.deinit();
    locmap.destroy();
    return false;
  }

  if (!lexer.run())
    return false;

  curt = lexer.tokens.arr-1;

  src->cacheLineOffsets();
  return true;
}

/* ----------------------------------------------------------------------------
 */
void Parser::deinit()
{
  locmap.destroy();
  lexer.deinit();
  *this = {};
}

/* ----------------------------------------------------------------------------
 */
void Parser::writeTokenSanitized()
{
  // Write out the current token with its control characters 
  // sanitized to lua's representation of them.
  for (u8 c : getRaw())
  {
    if (iscntrl(c))
      writeOut("\\"_str, c);
    else if (c == '"')
      writeOut("\\\""_str);
    else if (c == '\\')
      writeOut("\\\\"_str);
    else 
      writeOut((char)c);
  }
}

/* ----------------------------------------------------------------------------
 */
template<typename... T>
void Parser::writeOut(T... args)
{
  bytes_written += io::formatv(out, args...);
}

/* ----------------------------------------------------------------------------
 */
b8 Parser::run()
{
  TRACE("begin\n");

  nextToken();

  writeOut(
    "local __metaenv_docspan = __metaenv.docspan\n",
    "local __metaenv_doc = __metaenv.doc\n",
    "local __metaenv_val = __metaenv.val\n",
    "local __metaenv_macro = __metaenv.macro\n",
    "local __metaenv_macro_immediate = __metaenv.macro_immediate\n");

  for (;;)
  {
    using enum Token::Kind;

    if (at(Eof))
    {
      TRACE("encountered EOF\n");
      break;
    }

    switch (curt->kind)
    {
      case Invalid:
        return false;

      case Whitespace:
        writeOut(
          "__metaenv_docspan("_str, curtIdx(), ")\n");
        nextToken();
        break;

      case Document:
        // TODO(sushi) consider just using offsets into the input file 
        //             here instead of a whole lua string.

        TRACE("placing document text: '", 
              io::SanitizeControlCharacters(getRaw()), "'\n");

        writeOut("__metaenv_docspan("_str, curtIdx(), ")\n");
        nextToken();
        break;

      case LuaBlock:
        TRACE("placing lua block: '", 
              io::SanitizeControlCharacters(getRaw()), "'\n");
        {
          String raw = getRaw();
          pushLocMap();
          for (s32 i = 0; i < raw.len; ++i)
          {
            if (i && raw.ptr[i-1] == '\n' && i != raw.len-1)
              pushLocMap(i,i);
          }
        }
        writeOut(getRaw(), "\n");
        nextToken();
        break;

      case LuaLine:
        TRACE("placing lua line: '", 
              io::SanitizeControlCharacters(getRaw()), "'\n");
        pushLocMap(-1, 0);
        writeOut(getRaw(), "\n");
        nextToken();
        break;

      case LuaInline:
        TRACE("placing lua inline: '", 
              io::SanitizeControlCharacters(getRaw()), "'\n");
        writeOut("__metaenv_val("_str, curtIdx(), ',');
        pushLocMap();
        writeOut(getRaw(), ")\n"_str);
        nextToken();
        break;

      case MacroSymbol:
      case MacroSymbolImmediate:
        {
          TRACE("encountered macro symbol\n");
          b8 is_immediate = curt->kind == MacroSymbolImmediate;
          pushLocMap();

          if (is_immediate)
          {
            writeOut("__metaenv_doc("_str, curtIdx(),
              ",__metaenv_macro_immediate("_str, curtIdx(), ",");

            nextSignificantToken(); // identifier
            writeOut('"', getRaw(), "\", ");
          }
          else
          {
            writeOut("__metaenv_macro("_str);
            writeOut(curtIdx(), ',');

            TRACE("getting id\n");
            nextSignificantToken(); // identifier
            TRACE("got id\n");

            writeOut('"', getRaw(), "\",");
          }

          TRACE("wrote name\n");

          if (curt->kind == MacroMethod)
          {
            writeOut("true,"_str);
            String mobj = getRaw().sub(0, curt->method_colon_offset);
            String mfun = getRaw().sub(curt->method_colon_offset+1, curt->len);
            pushLocMap();
            writeOut(mobj, '.', mfun, ',', mobj);
          }
          else
          {
            writeOut("false,"_str);
            pushLocMap();
            writeOut(getRaw());
          }

          TRACE("wrote method\n");

          nextSignificantToken();
          if (at(MacroTupleArg))
          {
            TRACE("at tuple args\n");
            for (;;)
            {
              TRACE("parsing tuple arg ", getRaw(), "\n");
              // TODO(sushi) it's possible we could lazily load macro arguments
              //             from the input file instead of creating lua 
              //             strings. I'm not really sure if this would be 
              //             more efficient but for cases where macros never
              //             get called it might be useful.
              // NOTE(sushi) used to use this MacroPart stuff here 
              //             but finding that it makes working with macro
              //             arguments too unintuitive. Later once we get to
              //             needing to use the mappings these would provide
              //             we should provide some sorta lpp.macro api whose
              //             use makes it apparent that the macro arguments
              //             are special types, not just strings.
              //             The primary problem is wanting to index tables
              //             using macro arguments, since these are tables
              //             it won't use __tostring.
              // NOTE(sushi) these are now being used again to report to the
              //             metaenvironment where macro arguments start
              //             so that the user can get this information.
              //             This information could probably be stored better
              //             in some C data, but I would want to split up
              //             Document and Macros into their own types
              //             distinct from Section at that point and I 
              //             dont wannnnnaa do that since this is already 
              //             here :). 
              // NOTE(sushi) we can probably just store this information 
              //             separately from the args and provide an 
              //             interface for accessing it. Similar to what we
              //             currently do to get at the file offset of a 
              //             macro arg, just dont wrap the args in MacroParts.
              writeOut(',', 
                "__metaenv.lpp.MacroPart.new(",
                '"', source->name, '"', ',',
                curt->loc,              ',',
                curt->loc + curt->len,  ',',
                '"');
              pushLocMap();
              writeTokenSanitized();
              writeOut('"', ')');
              nextSignificantToken();
              if (not at(MacroTupleArg))
                break;
            }
          }
          else if (at(MacroStringArg))
          {
            TRACE("at string arg\n");
            writeOut(',',
                "__metaenv.lpp.MacroPart.new(",
                '"', source->name, '"', ',',
                curt->loc,              ',',
                curt->loc + curt->len,  ',',
                '"');
            pushLocMap();
            writeTokenSanitized();
            writeOut('"', ')');
            nextToken();
          }

          if (is_immediate)
            writeOut(')');
          writeOut(")\n"_str);
        }
        break;
    }
  }

  return true;
}

/* ----------------------------------------------------------------------------
 */
void Parser::pushLocMap(s32 from_offset, s32 to_offset)
{
  LocMapping mapping = 
  {
    *curt,
    curt->loc+from_offset,
    bytes_written+to_offset+1
  };

  locmap.push(mapping);
}

/* ----------------------------------------------------------------------------
 */
b8 Parser::nextToken()
{
  curt += 1;
  TRACE("found ", *curt, "\n");
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Parser::nextSignificantToken()
{
  for (;;)
  {
    nextToken();
    if (not at(Token::Kind::Whitespace))
      break;
  }
  return true;
}

/* ----------------------------------------------------------------------------
 */
b8 Parser::at(Token::Kind kind)
{
  return curt->kind == kind;
}

/* ----------------------------------------------------------------------------
 */
String Parser::getRaw()
{
  return curt->getRaw(source);
}

}
