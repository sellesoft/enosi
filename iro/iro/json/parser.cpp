#include "parser.h"
#include "../logger.h"

#include "stdlib.h"
#include "assert.h"

namespace iro::json
{

static auto logger = 
  Logger::create("json.parser"_str, Logger::Verbosity::Notice);

template<typename... T>
b8 Parser::errorHere(T... args)
{
  auto result = lexer.cache.asStr().findLineAndColumnAtOffset(curt.loc);
  ERROR(
      lexer.stream_name, ":", result.line, ":", result.column, ": ", 
      args..., "\n");
  return false;
}

template<typename... T>
b8 Parser::errorAt(s64 line, s64 column, T... args)
{
  ERROR(lexer.stream_name, ":", line, ":", column, ": ", args..., "\n");
  return false;
}

template<typename... T>
b8 Parser::errorNoLocation(T... args)
{
  ERROR(args..., "\n");
  return false;
}

/* ----------------------------------------------------------------------------
 */
b8 Parser::init(
    io::IO*  input_stream, 
    JSON*    json, 
    str      stream_name,
    jmp_buf* failjmp)
{
  assert(input_stream && json);

  TRACE(
    "initializing with input stream ", (void*)input_stream, 
    " and json at addr ", (void*)json, "\n");
  SCOPED_INDENT;

  in = input_stream;
  if (!lexer.init(input_stream, stream_name, failjmp))
    return false;

  this->json = json;
  this->failjmp = failjmp;

  if (!json->init())
    return false;

  value_stack = ValueStack::create();

  return true;
}

/* ----------------------------------------------------------------------------
 */
void Parser::deinit()
{
  lexer.deinit();
  *this = {};
}

/* ----------------------------------------------------------------------------
 */
b8 Parser::at(TKind kind)
{
  return curt.kind == kind;
}

/* ----------------------------------------------------------------------------
 */
void Parser::nextToken()
{
  curt = lexer.nextToken();
}

/* ----------------------------------------------------------------------------
 */
b8 Parser::start()
{
  using enum Token::Kind;

  nextToken();
  if (at(Eof))
    return true;

  if (!value())
    return false;

  json->root = value_stack.pop();

  if (at(Eof))
    return true;
  else
    return errorHere("more than one value given in JSON text");
}

/* ----------------------------------------------------------------------------
 *  Note that this leaves the resulting value at the top of the stack so that 
 *  the calling function may use it if needed. The caller must remove it 
 *  manually.
 */
b8 Parser::value()
{
  switch (curt.kind)
  {
    case TKind::Null:
      if(Value* v = json->newValue(VKind::Null))
      {
        value_stack.push(v);
        nextToken();
        return true;
      }
      else
        return errorHere("failed to create value for null token");

    case TKind::False:
    case TKind::True:
      if (Value* v = json->newValue(VKind::Boolean))
      {
        v->boolean = curt.kind == TKind::True;

        value_stack.push(v);
        nextToken();
        return true;
      }
      else
        return errorHere("failed to create value for boolean token");

    case TKind::Number:
      if (Value* v = json->newValue(VKind::Number))
      {
        // TODO(sushi) handle this more gracefully later
        //             like just write our own impl of strtod that takes 
        //             length
        u8 terminated[24];
        if (!lexer.getRaw(curt).nullTerminate(terminated, 24))
          return errorHere(
              "number value was too large to null-terminate, we only allow "
              "numbers up to 24 characters in length at the moment");

        v->number = strtod((char*)terminated, nullptr);

        value_stack.push(v);
        nextToken();
        return true;
      }
      else
        return errorHere("failed to create value for number token");

    case TKind::String:
      if (Value* v = json->newValue(VKind::String))
      {
        str s = lexer.getRaw(curt);

        // TODO(sushi) do better later
        io::Memory buffer;
        buffer.open(s.len);
        defer { buffer.close(); };

        for (s32 i = 0; i < s.len; ++i)
        {
          if (s.bytes[i] != '\\')
          {
            buffer.write({s.bytes+i, 1});
            continue;
          }

          i += 1;
          
          switch (s.bytes[i])
          {
          case '"':  io::format(&buffer, '"');  break;
          case '/':  io::format(&buffer, '/');  break;
          case '\\': io::format(&buffer, '\\'); break;
          case 'b':  io::format(&buffer, '\b'); break;
          case 'f':  io::format(&buffer, '\f'); break;
          case 'n':  io::format(&buffer, '\n'); break;
          case 'r':  io::format(&buffer, '\r'); break;
          case 't':  io::format(&buffer, '\t'); break;
          case 'u':
            {
              assert(!"TODO(sushi) implement unicode stuff");
            }
            break;
          default:
            assert(!"invalid escape sequence");
            break;
          }
        }

        v->string = json->cacheString(buffer.asStr());
        
        value_stack.push(v);
        nextToken();
        return true;
      }
      else
        return errorHere("failed to create value for string token");

    case TKind::LeftBrace:
      if (Value* v = json->newValue(VKind::Object))
      {
        value_stack.push(v);
        nextToken();

        if (!object())
          return false;

        return true;
      }
      else
        return errorHere("failed to create value for object");

    case TKind::LeftSquare:
      if (Value* v = json->newValue(VKind::Array))
      {
        value_stack.push(v);
        nextToken();
        
        if (!array())
          return false;

        return true;
      }
      else
        return errorHere("failed to create value for array");
  
    default: 
      return errorHere("expected a value");
  }
}

/* ------------------------------------------------------------------------------------------------
 */
b8 Parser::object()
{
  Value*  v = value_stack.head->data;
  Object* o = &v->object;

  if (!o->init())
    return errorHere("failed to initialize json::Object");

  if (at(TKind::RightBrace))
  {
    nextToken();
    return true;
  }

  for (;;)
  {
    if (!at(TKind::String))
      return errorHere(
          "expected a '}' or string for object member name (remember that "
          "trailing commas are not allowed in JSON!)");

    str member_name = json->cacheString(lexer.getRaw(curt));

    nextToken();
    if (!at(TKind::Colon))
      return errorHere("expected a ':' to separate member name and value");

    nextToken();
    if (!value())
      return false;

    Value* member_value = value_stack.head->data;
    value_stack.pop();

    if (!o->addMember(member_name, member_value))
      return errorHere("failed to add member '", member_name, "' to object");

    if (at(TKind::RightBrace))
      break;
  
    if (!at(TKind::Comma))
      return errorHere(
          "expected a '}' to end object or ',' to start a new member");

    nextToken();
  }

  nextToken();
  return true;
}

/* ------------------------------------------------------------------------------------------------
 */
b8 Parser::array()
{
  Value* v = value_stack.head->data;
  Array* a = &v->array;

  if (!a->init())
    return errorHere("failed to initialize json::Array");

  if (at(TKind::RightSquare))
  {
    nextToken();
    return true;
  }

  for (;;)
  {
    if (!value())
      return false;

    Value* array_value = value_stack.pop();
    
    a->push(array_value);
    
    if (at(TKind::RightSquare))
      break;

    if (!at(TKind::Comma))
      return errorHere(
          "expected a ']' to end array or ',' delimit new array member");

    nextToken();
  }

  nextToken();
  return true;
}

} // namespace json
