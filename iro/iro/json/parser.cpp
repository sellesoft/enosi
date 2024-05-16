#include "parser.h"
#include "../logger.h"

#include "stdlib.h"
#include "assert.h"

namespace iro::json
{

/* ------------------------------------------------------------------------------------------------ json::Parser::init
 */
b8 Parser::init(io::IO* input_stream, JSON* json_, str stream_name, Logger::Verbosity v)
{
    assert(input_stream && json_);

    logger.init("json.parser"_str, v);

    INFO("initializing with input stream ", (void*)input_stream, " and json at addr ", (void*)json_, "\n");
    SCOPED_INDENT;

    in = input_stream;
	if (!lexer.init(input_stream, stream_name))
		return false;

	json = json_;

	if (!json->init())
		return false;

	value_stack = ValueStack::create();

	return true;
}

/* ------------------------------------------------------------------------------------------------ json::Parser::deinit
 */
void Parser::deinit()
{
	lexer.deinit();
	
}

/* ------------------------------------------------------------------------------------------------ json::Parser::at
 */
b8 Parser::at(TKind kind)
{
	return curt.kind == kind;
}

/* ------------------------------------------------------------------------------------------------ json::Parser::nextToken
 */
void Parser::nextToken()
{
	curt = lexer.nextToken();
}

/* ------------------------------------------------------------------------------------------------ json::Parser::start
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

/* ------------------------------------------------------------------------------------------------ json::Parser::value
 *  Note that this leaves the resulting value at the top of the stack so that the calling function
 *  may use it if needed. The caller must remove it manually.
 */
b8 Parser::value()
{
	switch (curt.kind)
	{
		#define literalCase(k) \
			case TKind::k: \
				if (Value* v = json->newValue(VKind::k)) \
				{ \
					value_stack.push(v); \
					nextToken(); \
					return true; \
				} \
				else \
					return errorHere("failed to create value for " STRINGIZE(k) " token");
		
		literalCase(Null);
		literalCase(False);
		literalCase(True);

		#undef literal_case

		case TKind::Number:
			if (Value* v = json->newValue(VKind::Number))
			{
				// TODO(sushi) handle this more gracefully later
				//             like just write our own impl of strtod that takes length
				u8 terminated[24];
				if (!curt.raw.nullTerminate(terminated, 24))
					return errorHere("number value was too large to null-terminate, we only allow numbers up to 24 characters in length at the moment");

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
				v->string = curt.raw;
				
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

/* ------------------------------------------------------------------------------------------------ json::Parser::object
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
			return errorHere("expected a '}' or string for object member name (remember that trailing commas are not allowed in JSON!)");

		str member_name = curt.raw;

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
			return errorHere("expected a '}' to end object or ',' to start a new member");

		nextToken();
	}

	nextToken();
	return true;
}

/* ------------------------------------------------------------------------------------------------ json::Parser::array
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
			return errorHere("expected a ']' to end array or ',' delimit new array member");

		nextToken();
	}

	nextToken();
	return true;
}

} // namespace json
