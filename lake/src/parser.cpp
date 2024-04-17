#include "parser.h"

#include "lake.h"
#include "lexer.h"

#include "assert.h"
#include "stdlib.h"

/* ================================================================================================
 *
 *   Common token constants that are inserted into the token stack
 *
 */

const Token identifier_lake   = {tok::Identifier, strl("lake"), 0, 0};
const Token identifier_cmd    = {tok::Identifier, strl("cmd"), 0, 0};
const Token identifier_concat = {tok::Identifier, strl("concat"), 0, 0};
const Token identifier_clivars = {tok::Identifier, strl("clivars"), 0, 0};

const Token punctuation_dot       = {tok::Dot, strl("."), 0, 0};
const Token punctuation_lparen    = {tok::ParenLeft, strl("("), 0 ,0};
const Token punctuation_rparen    = {tok::ParenRight, strl(")"), 0, 0};
const Token punctuation_comma     = {tok::Comma, strl(","), 0, 0};
const Token punctuation_equal     = {tok::Equal, strl("="), 0, 0 };
const Token punctuation_lbrace    = {tok::BraceLeft, strl("{"), 0, 0};
const Token punctuation_rbrace    = {tok::BraceRight, strl("}"), 0, 0};
// const Token punctuation_dotdouble = {tok::DotDouble, strl(".."), 0, 0};

const Token keyword_local = {tok::Local, strl("local"), 0, 0};
const Token keyword_or    = {tok::Or, strl("or"), 0, 0};

const Token whitespace_space = {tok::Whitespace, strl(" "), 0, 0};


/* ================================================================================================
 *
 *   TokenStack
 *
 */

/* ------------------------------------------------------------------------------------------------
 */
void Parser::TokenStack::init()
{
	space = 32;
	len = 0;
	arr = (Token*)mem.allocate(sizeof(Token) * space);
}
  
void Parser::TokenStack::destroy()
{
	space = len = 0;
	mem.free(arr);
}
  
/* ------------------------------------------------------------------------------------------------
 */
void Parser::TokenStack::grow_if_needed(const s32 new_elements)
{
	if (len + new_elements <= space)
		return;

	while (len + new_elements > space) 
		space *= 2;

	arr = (Token*)mem.reallocate(arr, sizeof(Token) * space);

}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::TokenStack::push(Token t)
{
	grow_if_needed(1);

	arr[len] = t;
	len += 1;
}

void Parser::TokenStack::insert(s64 idx, Token t)
{
	assert(idx <= len);

	grow_if_needed(1);

	if (!len) push(t);
	else
	{
		mem.move(arr + idx + 1, arr + idx, sizeof(Token) * (len - idx));
		len += 1;
		arr[idx] = t;
	}
}

Token Parser::TokenStack::pop() 
{
	len -= 1;
	return arr[len];
}

/* ------------------------------------------------------------------------------------------------
 */
Token Parser::TokenStack::get_last_identifier()
{

	Token* search = arr + len - 1;
	for (;;)
	{
		if (search->kind == tok::Identifier)
		{
			return *search;
		}
		else if (search == arr)
			return {tok::Eof};
		search -= 1;
	}
}

/* ------------------------------------------------------------------------------------------------
 */
b8 Parser::TokenStack::insert_before_last_identifier(Token t)
{
	Token* search = arr + len - 1;
	for (;;)
	{
		if (search->kind == tok::Identifier)
		{
			insert(search - arr, t);
			return true;
		}
		else if (search == arr) 
			return false;
		search -= 1;
	}
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::TokenStack::push_before_whitespace(Token t)
{
	if (arr[len-1].kind == Whitespace)
	{
		insert(len-1, t);
	}
	else
	{
		push(t);
	}
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::TokenStack::print()
{
	for (u32 i = 0; i < len; i++)
	{
		Token t = arr[i];
		if (t.kind == tok::Whitespace)
			printv("Whitespace\n");
		else
			printv(tok_strings[(u32)t.kind], " ", t.raw, "\n");
	}
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::TokenStack::print_src()
{
	for (u32 i = 0; i < len; i++)
	{
		Token t = arr[i];
		if (t.kind == String)
			printv("\"", t.raw, "\"");
		else
			::print(t.raw);
	}
}


/* ------------------------------------------------------------------------------------------------
 */
dstr Parser::fin()
{
	dstr out = dstr::create();

	for (u32 i = 0; i < stack.len; i++)
	{
		Token t = stack.arr[i];
		if (t.kind == String)
			out.appendv("\"", t.raw, "\"");
		else
			out.append(t.raw);
	}

	return out;
}

/* ================================================================================================
 *
 *  Parsing
 *  
 */

/* ------------------------------------------------------------------------------------------------
 */
void Parser::init(Lake* lake_)
{
	lake = lake_;
	lex = (Lexer*)mem.allocate(sizeof(Lexer));
	lex->init(lake);
	curt = {};
	stack.init();
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::destroy()
{
	mem.free(lex);
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
	next_token();
	chunk();
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::chunk()
{
	for (;;)
	{
		if (statement() || block_follow(curt))
			break;
		if (at(Semicolon))
			next_token();
	}
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::block()
{
	chunk();
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::funcargs()
{
	Token save = curt;
	switch (curt.kind)
	{
		case ParenLeft: {
			next_token();

			if (at(ParenRight))
			{
				next_token();
				break;
			}

			exprlist1();

			if (!at(ParenRight))
				error_here("expected a ')' to match '(' at ", save.line, ":", save.column);
			next_token();
		} break;

		case BraceLeft: {
			tableconstructor();
		} break;

		case String: {
			next_token();
		} break;

		default: {
			error_here("expected function argument list, table, or string");
		} break;
	}
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::parlist()
{
	if (at(ParenRight))
		return;

	for (;;)
	{
		if (at(Ellipses))
			break;
		else if (!at(Identifier))
			error_here("expected an identifier or '...' as function arg");
		next_token();

		if (!at(Comma))
			break;
		next_token();
	}
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::field()
{
	next_token();
	
	if (!at(Identifier))
		error_here("expected an identifier after '.' or ':'");
	next_token();
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::body(Token start)
{
	if (!at(ParenLeft))
		error_here("expected '(' to start function parameter list");
	next_token();

	parlist();

	if (!at(ParenRight))
		error_here("expected ')' to close function parameter list");
	next_token();

	chunk();

	if (!at(End))
		error_here("expected 'end' to match 'function' at ", start.line, ":", start.column);
	next_token();
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::yindex()
{
	Token save = curt;
	next_token();

	expr();

	if (!at(SquareRight))
		error_here("expected a ']' to end '[' at ", save.line, ":", save.column);
	next_token();
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::simpleexpr()
{
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
			next_token(); 
			body(save);
			return;
		default:
			primaryexpr();   
			return; 
	}
	next_token();  
}

/* ------------------------------------------------------------------------------------------------
 */
struct bop_prio
{
	u8 left, right;
};

bop_prio get_priority(Token t)
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

	error(strl(""), t.line, t.column, "INTERNAL ERROR SOMEHOW WE FAILED TO GET A BINARY OP IN get_priority() ????");
	exit(1);
}

#define UOP_PRIO 8

/* ------------------------------------------------------------------------------------------------
 */
void Parser::subexpr(s32 limit)
{
	if (is_uop(curt))
	{
		next_token();
		subexpr(UOP_PRIO);
	}
	else
		simpleexpr();

	while (is_bop(curt)) 
	{
		auto prio = get_priority(curt);
		if (prio.left < limit)
			break;
		next_token();
		subexpr(prio.right); 
	}
}  

/* ------------------------------------------------------------------------------------------------
 */
void Parser::expr()
{
	subexpr(0);
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::prefixexpr()
{
	switch (curt.kind)
	{
		case ParenLeft: {
			Token save = curt;
			next_token();
			
			expr();
			if (!at(ParenRight))
				error_here("expected ')' to end '(' at ", save.line, ":", save.column);
			next_token();
		} break;

		/* @lakesyntax
		 */
		case Dollar: {
			next_token(false);

			if (!at(SquareLeft))
				error_here("expected '[' after '$' to begin whitespace delimited string array");
			next_token(false);

			stack.push_before_whitespace(punctuation_lbrace);

			if (at(SquareRight))
			{
				warn_here("empty whitespace delimited array eg. $[]");
				stack.push(punctuation_rbrace);
				next_token(false);
				break;
			}
			
			for (;;)
			{
				Token save = curt;

				for (;;)
				{
					curt = lex->next_token();
					if (curt.kind == Whitespace ||
						curt.kind == SquareRight)
						break;
				}

				stack.push({String, {save.raw.s, (s32)(curt.raw.s - save.raw.s)}, 0, 0});
				stack.push(punctuation_comma);

				if (curt.kind == SquareRight)
					break;

				next_token();

				if (curt.kind == SquareRight) // :/
					break;
			}

			stack.push(punctuation_rbrace);
			next_token(false);
		} break;

		/* @lakesyntax
		*/
		case Backtick: {
			stack.push(identifier_lake);
			stack.push(punctuation_dot);
			stack.push(identifier_cmd);
			stack.push(punctuation_lparen);

			next_token(false,false);

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
						error_here("encountered end of file while consuming command argument that began at ", save.line, ":", save.column);
					curt = lex->next_token();
				}
			};

			for (;;)
			{
				switch (curt.kind)
				{
					case Dollar: {
						Token save = curt;

						next_token(false,false);

						if (!at(ParenLeft))
						{
							consume_arg();
							stack.push({String, {save.raw.s, (s32)(curt.raw.s - save.raw.s)}, 0, 0});
							break;
						}

						next_token(false,false);
						expr();

						if (!at(ParenRight))
							error_here("expected ')' to end interpolated command argument started at ", save.line, ":", save.column);
						next_token(false,false);
					} break;

					default: {
						Token save = curt;
						consume_arg();
						stack.push({String, {save.raw.s, (s32)(curt.raw.s - save.raw.s)}, 0, 0});
						if (at(Whitespace))
							next_token(false,false);
					} break;

					case Backtick:
						break;
				}

				if (at(Backtick))
					break;

				stack.push(punctuation_comma);
			}

			if (stack.arr[stack.len-1].kind == Comma)
				stack.pop(); // kind silly

			stack.push(punctuation_rparen);
			next_token(false);
		} break;

		case Identifier: 
			next_token();
			break;

		default: {
			error_here("expected an identifier or '('");
		} break;
	}
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::primaryexpr(b8* is_call)
{
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
				next_token();

				if (!at(Identifier))
					error_here("expected an identifier after method call ':'");
				next_token();

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
	expr();
	while (at(Comma))
	{
		next_token();
		expr();
	}
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::recfield()
{
	if (at(Identifier))
	{
		next_token(); 
	}
	else
		yindex();

	if(!at(Equal))
		error_here("expected '=' for table key");
	next_token();

	expr();
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::tableconstructor()
{
	Token save = curt;

	if (!at(BraceLeft))
		error_here("expected '{' for table constructor");
	next_token();

	for (;;)
	{
		if (at(BraceRight))
			break;

		switch (curt.kind)
		{
			case Identifier: {
				lookahead();
				if (!lookahead_at(Equal)) 
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
		next_token();
	}

	if (!at(BraceRight))
		error_here("expected '}' to end table started at ", save.line, ":", save.column);
	next_token();
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::assignment()
{
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
				if (!stack.insert_before_last_identifier(keyword_local))
					error_here("encountered '?=' but could not find preceeding identifier to place 'local'.");
				stack.insert_before_last_identifier(whitespace_space);

				Token last_identifier = stack.get_last_identifier();
			
				// TODO(sushi) once we actually gather cli args just do the replacement directly
				//             here instead of this silly function call thing
				stack.push(punctuation_equal);
				stack.push(whitespace_space);
				stack.push(identifier_lake);
				stack.push(punctuation_dot);
				stack.push(identifier_clivars);
				stack.push(punctuation_dot);
				stack.push(last_identifier);
				stack.push(whitespace_space);
				stack.push(keyword_or);
				stack.push(whitespace_space);
				stack.push(punctuation_lparen);
				
				next_token(false);
				exprlist1();

				stack.push_before_whitespace(punctuation_rparen);
			} break;

			case ColonEqual: {
				if (!stack.insert_before_last_identifier(keyword_local))
					error_here("encountered ':=' but could not find preceeding identifier to place 'local'.");
				stack.insert_before_last_identifier(whitespace_space);

				stack.push(punctuation_equal);

				next_token(false);
				exprlist1();
			} break;

			case DotDoubleEqual: {
				Token last_identifier = stack.get_last_identifier();
				
				// call a concat function that handles tables and strings
				stack.push(punctuation_equal);
				stack.push(whitespace_space);
				stack.push(identifier_lake);
				stack.push(punctuation_dot);
				stack.push(identifier_concat);
				stack.push(punctuation_lparen);
				stack.push(last_identifier);
				stack.push(punctuation_comma);

				next_token(false);
				exprlist1();

				stack.push_before_whitespace(punctuation_rparen);
			} break;

			case Equal: {
				next_token();
				exprlist1();
			} break;
		}
	}
}

/* ------------------------------------------------------------------------------------------------
 */
void Parser::exprstat()
{
	b8 is_call = false;
	primaryexpr(&is_call);
	if (!is_call)
		assignment();
}

/* ------------------------------------------------------------------------------------------------
 */
b8 Parser::statement()
{
	Token save = curt;
	switch (curt.kind)
	{
		case If: {
			next_token();
			expr();

			if (!at(Then))
				error_here("expected 'then' after if condition");
			next_token();

			block();

			while (at(ElseIf))
			{
				expr();
				if (!at(Then))
					error_here("expected 'then' after elseif condition");
				next_token();
			}

			if (at(Else))
			{
				next_token();
				block();
			}

			if (!at(End))
				error_here("expected 'end' to match if at ", save.line, ":", save.column);
			next_token();
		} return false;

		case While: {
			next_token();
			expr();

			if (!at(Do))
				error_here("expected 'do' after while condition");
			next_token();

			block();

			if (!at(End))
				error_here("expected 'end' to match while at ", save.line, ":", save.column);
		} return false;

		case Do: {
			next_token();
			block();
			if (!at(End))
				error_here("expected 'end' to match 'do' at ", save.line, ":", save.column);
		} return false;

		case For: {
			next_token();
			next_token(); // should just be a var name
			switch (curt.kind)
			{
				case Equal: {
					next_token();
					expr();
					if (at(Comma))
					{
						next_token();
						expr();
					}
				} break;
				
				case In: {
					next_token();
					exprlist1();
				} break;

				case Comma: {
					next_token();

					while (at(Comma))
					{
						next_token();
						if (!at(Identifier))
							error_here("expected an identifier");
					}
					next_token();

					if (!at(In))
						error_here("expected 'in' for list 'for' statement");
					next_token();

					exprlist1();
				} break;
			}

			if (!at(Do))
				error_here("expected 'do' after for loop");
			next_token();

			block();
			
			if (!at(End))
				error_here("expected 'end' to match 'for' at ", save.line, ":", save.column);
			next_token();
		} return false;

		case Repeat: {
			// TODO(sushi) IDK if i will ever use it but support later 
			assert(!"repeat is not implemented yet!");
		} return false;

		case Function: {
			next_token();

			while (at(Dot))
				field();

			if (at(Colon))
				field();
			next_token();

			body(save);
		} return false;

		case Local: {
			next_token();

			if (at(Function))
				body(save);
			else
			{
				next_token();

				while(at(Comma))
					next_token();
				
				next_token();

				if (at(Equal))
					exprlist1();
			}
		} return false;

		case Return: {
			next_token();

			if (block_follow(curt))
				break;

			exprlist1();
		} return true;
		
		case Break: {
			next_token();
		} return true;

		default: {
			exprstat();
		} return false;
	}
	
	assert(!"unhandled statement case somehow ?? ");
	return false;
}

void Parser::next_token(b8 push_on_stack, b8 push_whitespace)
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
		curt = lex->next_token();
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
		lookahead_token = lex->next_token();
		if (lookahead_at(Whitespace))
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

b8 Parser::lookahead_at(tok k)
{
	return lookahead_token.kind == k;
}
