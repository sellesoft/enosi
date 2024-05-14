#include "parser.h"

#include "lake.h"
#include "lexer.h"

#include "assert.h"
#include "stdlib.h"

#include "logger.h"

#define PARSER_TRACE \
	TRACE(__func__, ": ", lex.getRaw(curt), "\n"); \
	SCOPED_INDENT;

/* ================================================================================================
 *
 *   Common token constants that are inserted into the token stack
 *
 */

#define defVirtTok(v, k, r) static const Parser::TokenStack::Elem::Virt v = {k, r}

defVirtTok(identifier_lake,    tok::Identifier, "lake"_str);
defVirtTok(identifier_cmd,     tok::Identifier, "cmd"_str);
defVirtTok(identifier_concat,  tok::Identifier, "concat"_str);
defVirtTok(identifier_clivars, tok::Identifier, "clivars"_str);

defVirtTok(punctuation_dot,    tok::Dot, "."_str);
defVirtTok(punctuation_lparen, tok::ParenLeft, "("_str);
defVirtTok(punctuation_rparen, tok::ParenRight, ")"_str);
defVirtTok(punctuation_comma,  tok::Comma, ","_str);
defVirtTok(punctuation_equal,  tok::Equal, "="_str);
defVirtTok(punctuation_lbrace, tok::BraceLeft, "{"_str);
defVirtTok(punctuation_rbrace, tok::BraceRight, "}"_str);

defVirtTok(keyword_local, tok::Local, "local"_str);
defVirtTok(keyword_or,    tok::Or, "or"_str);

defVirtTok(whitespace_space, tok::Whitespace, " "_str);

#undef defVirtTok

/* ================================================================================================
 *
 *   TokenStack
 *
 */

/* ------------------------------------------------------------------------------------------------
 */
void Parser::TokenStack::init(mem::Allocator* allocator)
{
	arr = Array<Elem>::create(16, allocator);
}
  
void Parser::TokenStack::destroy()
{
	arr.destroy();
}
  
/* ------------------------------------------------------------------------------------------------
 */
void Parser::TokenStack::push(Token t)
{
	arr.push({.is_virtual = false, .real = t});
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::TokenStack::push(Elem::Virt v)
{
	arr.push({.is_virtual = true, .virt = v});
}

void Parser::TokenStack::insert(s64 idx, Token t)
{
	arr.insert(idx, {.is_virtual = false, .real = t});
}

void Parser::TokenStack::insert(s64 idx, Elem::Virt v)
{
	arr.insert(idx, {.is_virtual = true, .virt = v});
}

void Parser::TokenStack::pop() 
{
	arr.pop();
}

/* ------------------------------------------------------------------------------------------------
 */
Token Parser::TokenStack::getLastIdentifier()
{
	Elem* search = arr.end() - 1;
	for (;;)
	{
		if (search->is_virtual)
			continue;
		if (search->real.kind == tok::Identifier)
		{
			return search->real;
		}
		else if (search == arr.begin())
			return {tok::Eof};
		search -= 1;
	}
}

/* ------------------------------------------------------------------------------------------------
 */
b8 Parser::TokenStack::insertBeforeLastIdentifier(Elem::Virt v)
{
	Elem* search = arr.end() - 1;
	for (;;)
	{
		if (search->is_virtual)
			continue;
		if (search->real.kind == tok::Identifier)
		{
			insert(search - arr.begin(), v);
			return true;
		}
		else if (search == arr.begin()) 
			return false;
		search -= 1;
	}
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::TokenStack::pushBeforeWhitespace(Elem::Virt v)
{
	if (!arr.last()->is_virtual && arr.last()->real.kind == Whitespace)
	{
		insert(arr.len()-1, v);
	}
	else
	{
		push(v);
	}
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::TokenStack::print(Parser* p, Logger logger)
{
	for (u32 i = 0; i < arr.len(); i++)
	{
		Elem e = arr[i];
		if (e.is_virtual)
		{
			INFO("Virtual: ", tok_strings[(u32)e.virt.kind], " ", e.virt.raw, "\n");
		}
		else
		{
			if (e.real.kind == Whitespace)
			{
				INFO("Whitespace\n");
			}
			else
			{
				INFO(tok_strings[(u32)e.real.kind], " ", p->lex.getRaw(e.real), "\n");
			}
		}
	}
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::TokenStack::printSrc(Parser* p, Logger logger)
{
	for (u32 i = 0; i < arr.len(); i++)
	{
		Elem e = arr[i];
		if (e.is_virtual)
		{
			INFO(e.virt.raw);
		}
		else
		{
			if (e.real.kind == String)
			{
				INFO('"', p->lex.getRaw(e.real), '"');
			}
			else
			{
				INFO(p->lex.getRaw(e.real));
			}
		}
	}
}


/* ------------------------------------------------------------------------------------------------
 */
Moved<io::Memory> Parser::fin()
{
	io::Memory out;
	out.open();

	for (u32 i = 0; i < stack.arr.len(); i++)
	{
		TokenStack::Elem e = stack.arr[i];
		if (e.is_virtual)
		{
			io::format(&out, e.virt.raw);
		}
		else
		{
			if (e.real.kind == String)
			{
				io::formatv(&out, '"', lex.getRaw(e.real), '"');
			}
			else
			{
				io::format(&out, lex.getRaw(e.real));
			}
		}
	}

	return move(out);
}

/* ================================================================================================
 *
 *  Parsing
 *  
 */

/* ------------------------------------------------------------------------------------------------
 */
void Parser::init(str sourcename, io::IO* in, Logger::Verbosity verbosity, mem::Allocator* allocator)
{
	lex.init(sourcename, in, verbosity);
	stack.init(allocator);
	logger = Logger::create("lake.parser"_str, verbosity);
	curt = {};
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::destroy()
{
	stack.destroy();
	lex.deinit();
	*this = {};
}

/* ------------------------------------------------------------------------------------------------
 *  Helpers
\  * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* ------------------------------------------------------------------------------------------------
 */
b8 block_follow(Token t)
{
	using enum tok;

	switch(t.kind)
	{
		case Else: case ElseIf: case End:
		case Until: case Eof:
			return true;
		default: return false; 
	}
}

/* ------------------------------------------------------------------------------------------------
 */
b8 is_uop(Token t)
{
	using enum tok;

	switch (t.kind)
	{
		case Not: 
		case Minus:
		case Pound:
			return true;
		default: return false;
	}
}

/* ------------------------------------------------------------------------------------------------
 */
b8 is_bop(Token t)
{
	using enum tok;

	switch (t.kind)
	{
		case Plus: case Minus:
		case Asterisk: case Solidus:
		case Percent: case Caret:
		case DotDouble: case TildeEqual:
		case EqualDouble: case LessThan:
		case LessThanOrEqual: case GreaterThan:
		case GreaterThanOrEqual: case And:
		case Or:
			return true;
		default: return false;
	}
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::start()
{
	PARSER_TRACE;
	nextToken();
	chunk();
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::chunk()
{
	PARSER_TRACE;
	for (;;)
	{
		if (statement() || block_follow(curt))
			break;
		if (at(Semicolon))
			nextToken();
	}
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::block()
{
	PARSER_TRACE;
	chunk();
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::funcargs()
{
	PARSER_TRACE;
	Token save = curt;
	switch (curt.kind)
	{
		case ParenLeft: {
			nextToken();

			if (at(ParenRight))
			{
				nextToken();
				break;
			}

			exprlist1();

			if (!at(ParenRight))
				errorHere("expected a ')' to match '(' at ", curt.line, ":", curt.column);
			nextToken();
		} break;

		case BraceLeft: {
			tableconstructor();
		} break;

		case String: {
			nextToken();
		} break;

		default: {
			errorHere("expected function argument list, table, or string");
		} break;
	}
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::parlist()
{
	PARSER_TRACE;
	if (at(ParenRight))
		return;

	for (;;)
	{
		if (at(Ellipses))
			break;
		else if (!at(Identifier))
			errorHere("expected an identifier or '...' as function arg");
		nextToken();

		if (!at(Comma))
			break;
		nextToken();
	}
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::field()
{
	PARSER_TRACE;
	nextToken();
	
	if (!at(Identifier))
		errorHere("expected an identifier after '.' or ':'");
	nextToken();
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::body(Token start)
{
	PARSER_TRACE;
	if (!at(ParenLeft))
		errorHere("expected '(' to start function parameter list");
	nextToken();

	parlist();

	if (!at(ParenRight))
		errorHere("expected ')' to close function parameter list");
	nextToken();

	chunk();

	if (!at(End))
		errorHere("expected 'end' to match 'function' at ", start.line, ":", start.column);
	nextToken();
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::yindex()
{
	PARSER_TRACE;
	Token save = curt;
	nextToken();

	expr();

	if (!at(SquareRight))
		errorHere("expected a ']' to end '[' at ", save.line, ":", save.column);
	nextToken();
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::simpleexpr()
{
	PARSER_TRACE;
	Token save = curt;
	switch (curt.kind)
	{
		case Number: 
		case String:
		case Nil:
		case True:
		case False:
		case Ellipses:
			break;
		case BraceLeft:
			tableconstructor();     
			return;  
		case Function: 
			nextToken(); 
			body(save);
			return;
		default:
			primaryexpr();   
			return; 
	}
	nextToken();  
}

/* ------------------------------------------------------------------------------------------------
 */
struct bop_prio
{
	u8 left, right;
};

bop_prio getPriority(Token t, Parser* p)
{
	using enum tok;

	switch (t.kind)
	{
		case Plus: case Minus:
			return {6, 6};
		case Solidus: case Percent: case Asterisk:
			return {7, 7};
		case Caret: 
			return {10, 9};
		case DotDouble:
			return {5, 4};
		case EqualDouble:     case TildeEqual:
		case LessThan:        case GreaterThan:
		case LessThanOrEqual: case GreaterThanOrEqual:
			return {3, 3};
		case And: 
			return {2, 2};
		case Or:
			return {1, 1};
	}

	p->errorHere("INTERNAL ERROR SOMEHOW WE FAILED TO GET A BINARY OP IN get_priority() ????");
	return {};
}

#define UOP_PRIO 8

/* ------------------------------------------------------------------------------------------------
 */
void Parser::subexpr(s32 limit)
{
	PARSER_TRACE;
	if (is_uop(curt))
	{
		nextToken();
		subexpr(UOP_PRIO);
	}
	else
		simpleexpr();

	while (is_bop(curt)) 
	{
		auto prio = getPriority(curt, this);
		if (prio.left < limit)
			break;
		nextToken();
		subexpr(prio.right); 
	}
}  

/* ------------------------------------------------------------------------------------------------
 */
void Parser::expr()
{
	PARSER_TRACE;
	subexpr(0);
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::prefixexpr()
{
	PARSER_TRACE;
	switch (curt.kind)
	{
		case ParenLeft: {
			Token save = curt;
			nextToken();
			
			expr();
			if (!at(ParenRight))
				errorHere("expected ')' to end '(' at ", save.line, ":", save.column);
			nextToken();
		} break;

		/* @lakesyntax
		 */
		case Dollar: {
			nextToken(false);

			if (!at(SquareLeft))
				errorHere("expected '[' after '$' to begin whitespace delimited string array");
			nextToken(false);

			stack.pushBeforeWhitespace(punctuation_lbrace);

			if (at(SquareRight))
			{
				warnHere("empty whitespace delimited array eg. $[]");
				stack.push(punctuation_rbrace);
				nextToken(false);
				break;
			}
			
			for (;;)
			{
				Token save = curt;

				for (;;)
				{
					curt = lex.nextToken();
					if (curt.kind == Whitespace ||
						curt.kind == SquareRight)
						break;
				}

				stack.push(Token{String, save.offset, curt.offset - save.offset});
				stack.push(punctuation_comma);

				if (curt.kind == SquareRight)
					break;

				nextToken();

				if (curt.kind == SquareRight) // :/
					break;
			}

			stack.push(punctuation_rbrace);
			nextToken(false);
		} break;

		/* @lakesyntax
		*/
		case Backtick: {
			stack.push(identifier_lake);
			stack.push(punctuation_dot);
			stack.push(identifier_cmd);
			stack.push(punctuation_lparen);

			nextToken(false,false);

			auto consume_arg = [this]()
			{
				Token save = curt;

				for (;;)
				{
					if (at(Whitespace) ||
						at(Dollar)     ||
						at(Backtick))
					{
						return;
					}

					if (at(Eof))
						errorHere("encountered end of file while consuming command argument that began at ", save.line, ":", save.column);
					nextToken();
				}
			};

			for (;;)
			{
				switch (curt.kind)
				{
					case Dollar: {
						Token save = curt;

						nextToken(false,false);

						if (!at(ParenLeft))
						{
							consume_arg();
							stack.push({String, save.offset, curt.offset - save.offset, 0});
							break;
						}

						nextToken(false,false);
						expr();

						if (!at(ParenRight))
							errorHere("expected ')' to end interpolated command argument started at ", save.line, ":", save.column);
						nextToken(false,false);
					} break;

					default: {
						Token save = curt;
						consume_arg();
						stack.push({String, save.offset, curt.offset - save.offset, 0});
						if (at(Whitespace))
							nextToken(false,false);
					} break;

					case Backtick:
						break;
				}

				if (at(Backtick))
					break;

				stack.push(punctuation_comma);
			}

			if (stack.arr[stack.arr.len()-1].real.kind == Comma)
				stack.pop(); // kind silly

			stack.push(punctuation_rparen);
			nextToken(false);
		} break;

		case Identifier: 
			nextToken();
			break;

		default: {
			errorHere("expected an identifier or '('");
		} break;
	}
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::primaryexpr(b8* is_call)
{
	PARSER_TRACE;
	prefixexpr();

	for (;;)
	{
		switch (curt.kind)
		{
			case Dot: {
				field();
			} break;

			case SquareLeft: {
				yindex();
			} break;

			case Colon: {
				nextToken();

				if (!at(Identifier))
					errorHere("expected an identifier after method call ':'");
				nextToken();

				funcargs();
				if (is_call) *is_call = true;
			} break;

			case ParenLeft:
			case String: 
			case BraceLeft: {
				funcargs();
				if (is_call) *is_call = true;
			} break;

			default: return;
		}
	}
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::exprlist1()
{
	PARSER_TRACE;
	expr();
	while (at(Comma))
	{
		nextToken();
		expr();
	}
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::recfield()
{
	PARSER_TRACE;
	if (at(Identifier))
	{
		nextToken(); 
	}
	else
		yindex();

	if(!at(Equal))
		errorHere("expected '=' for table key");
	nextToken();

	expr();
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::tableconstructor()
{
	PARSER_TRACE;
	Token save = curt;

	if (!at(BraceLeft))
		errorHere("expected '{' for table constructor");
	nextToken();

	for (;;)
	{
		if (at(BraceRight))
			break;

		switch (curt.kind)
		{
			case Identifier: {
				lookahead();
				if (!lookaheadAt(Equal)) 
					expr();
				else
					recfield();
			} break; 

			case SquareLeft: 
				recfield();
				break;
			default:
				expr();
				break;
		} 

		if (!at(Comma) && !at(Semicolon))
			break;
		nextToken();
	}

	if (!at(BraceRight))
		errorHere("expected '}' to end table started at ", save.line, ":", save.column);
	nextToken();
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::assignment()
{
	PARSER_TRACE;
	if (at(Comma))
	{
		primaryexpr();
		assignment();
	}
	else
	{
		switch(curt.kind)
		{
			case QuestionMarkEqual: {
				if (!stack.insertBeforeLastIdentifier(keyword_local))
					errorHere("encountered '?=' but could not find preceeding identifier to place 'local'.");
				stack.insertBeforeLastIdentifier(whitespace_space);

				Token last_identifier = stack.getLastIdentifier();
			
				stack.pushv(
					punctuation_equal, // = lake.clivars.<lastid> or (
					whitespace_space,
					identifier_lake,
					punctuation_dot,
					identifier_clivars,
					punctuation_dot,
					last_identifier,
					whitespace_space,
					keyword_or,
					whitespace_space,
					punctuation_lparen);
				
				nextToken(false);
				exprlist1();

				stack.pushBeforeWhitespace(punctuation_rparen);
			} break;

			case ColonEqual: {
				if (!stack.insertBeforeLastIdentifier(keyword_local))
					errorHere("encountered ':=' but could not find preceeding identifier to place 'local'.");
				stack.insertBeforeLastIdentifier(whitespace_space);

				stack.push(punctuation_equal);

				nextToken(false);
				exprlist1();
			} break;

			case DotDoubleEqual: {
				// TODO(sushi) this does not work in cases like a table access eg. files.c will return just c
				//             we'll need to store more complex information to get this to work properly i think
				Token last_identifier = stack.getLastIdentifier();
				
				// call a concat function that handles tables and strings
				stack.pushv(
	 				punctuation_equal, // = lake.concat(<lastid>,
					whitespace_space,
					identifier_lake,
					punctuation_dot,
					identifier_concat,
					punctuation_lparen,
					last_identifier,
					punctuation_comma);

				nextToken(false);
				exprlist1();

				stack.pushBeforeWhitespace(punctuation_rparen);
			} break;

			case Equal: {
				nextToken();
				exprlist1();
			} break;
		}
	}
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::exprstat()
{
	PARSER_TRACE;
	b8 is_call = false;
	primaryexpr(&is_call);
	if (!is_call)
		assignment();
}

/* ----------------------H------------------------------------------------------------------------
 */
b8 Parser::statement()
{
	PARSER_TRACE;
	Token save = curt;
	switch (curt.kind)
	{
		case If: {
			nextToken();
			expr();

			if (!at(Then))
				errorHere("expected 'then' after if condition");
			nextToken();

			block();

			while (at(ElseIf))
			{
				expr();
				if (!at(Then))
					errorHere("expected 'then' after elseif condition");
				nextToken();
			}

			if (at(Else))
			{
				nextToken();
				block();
			}

			if (!at(End))
				errorHere("expected 'end' to match if at ", save.line, ":", save.column);
			nextToken();
		} return false;

		case While: {
			nextToken();
			expr();

			if (!at(Do))
				errorHere("expected 'do' after while condition");
			nextToken();

			block();

			if (!at(End))
				errorHere("expected 'end' to match while at ", save.line, ":", save.column);
		} return false;

		case Do: {
			nextToken();
			block();
			if (!at(End))
				errorHere("expected 'end' to match 'do' at ", save.line, ":", save.column);
		} return false;

		case For: {
			nextToken();
			nextToken(); // should just be a var name
			switch (curt.kind)
			{
				case Equal: {
					nextToken();
					expr();
					if (at(Comma))
					{
						nextToken();
						expr();
					}
				} break;
				
				case In: {
					nextToken();
					exprlist1();
				} break;

				case Comma: {
					nextToken();

					while (at(Comma))
					{
						nextToken();
						if (!at(Identifier))
							errorHere("expected an identifier");
					}
					nextToken();

					if (!at(In))
						errorHere("expected 'in' for list 'for' statement");
					nextToken();

					exprlist1();
				} break;
			}

			if (!at(Do))
				errorHere("expected 'do' after for loop");
			nextToken();

			block();
			
			if (!at(End))
				errorHere("expected 'end' to match 'for' at ", save.line, ":", save.column);
			nextToken();
		} return false;

		case Repeat: {
			// TODO(sushi) IDK if i will ever use it but support later 
			assert(!"repeat is not implemented yet!");
		} return false;

		case Function: {
			nextToken();

			while (at(Dot))
				field();

			if (at(Colon))
				field();
			nextToken();

			body(save);
		} return false;

		case Local: {
			nextToken();

			if (at(Function))
				body(save);
			else
			{
				nextToken();

				while(at(Comma))
					nextToken();
				
				nextToken();

				if (at(Equal))
					exprlist1();
			}
		} return false;

		case Return: {
			nextToken();

			if (block_follow(curt))
				break;

			exprlist1();
		} return true;
		
		case Break: {
			nextToken();
		} return true;

		default: {
			exprstat();
		} return false;
	}
	
	assert(!"unhandled statement case somehow ?? ");
	return false;
}

void Parser::nextToken(b8 push_on_stack, b8 push_whitespace)
{
	if (has_lookahead)
	{
		has_lookahead = false;
		curt = lookahead_token;
		return;
	}

	if (push_on_stack)
		stack.push(curt);
	for (;;)
	{
		curt = lex.nextToken();
		if (at(Whitespace))
		{
			if (push_whitespace)
				stack.push(curt);
		}
		else
			break;
	}
}

void Parser::lookahead(b8 push_on_stack, b8 push_whitespace)
{
	if (has_lookahead)
		return;

	has_lookahead = true;

	if (push_on_stack)
		stack.push(curt);
	for (;;)
	{
		lookahead_token = lex.nextToken();
		if (lookaheadAt(Whitespace))
		{
			if (push_whitespace)
				stack.push(lookahead_token);
		}
		else
			break;
	}
}

b8 Parser::at(tok k)
{
	return curt.kind == k;
}

b8 Parser::lookaheadAt(tok k)
{
	return lookahead_token.kind == k;
}
