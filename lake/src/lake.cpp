#include "lake.h"
#include "platform.h"

#include "stdlib.h"
#include "assert.h"

#include "lexer.h"
#include "parser.h"

Lake lake;

void Lake::init(str p)
{
	path = p;
	parser = (Parser*)mem.allocate(sizeof(Parser));
	parser->init(this);
}

void Lake::run()
{
	parser->start();
}

/*

// advances curt until we come across an arg delimiter
b8 backtick_consume_arg(Lake* lake, Lexer* lex, TokenStack& stack, Token& curt)
{
	using enum tok;
	
	Token save = curt;

	for (;;)
	{
		if (   curt.kind == Whitespace 
		    || curt.kind == Dollar 
			|| curt.kind == Backtick)
		{
			return true;
		}

		if (curt.kind == Eof)
		{
			error(lake->path, save.line, save.column, "encountered end of file while consuming command");
			exit(1);
		}

		curt = lex->next_token();
	}
}

b8 backtick_dollar(Lake* lake, Lexer* lex, TokenStack& stack, Token& curt)
{
	using enum tok;

	Token save = curt;

	curt = lex->next_token();

	if (curt.kind != ParenLeft)
	{
		if (!backtick_consume_arg(lake, lex, stack, curt))
			return false;
		
		stack.push({String, {save.raw.s, (s32)(curt.raw.s - save.raw.s)}, 0, 0});
		return true;
	}

	for (;;)
	{
		curt = lex->next_token();

		if (curt.kind == ParenRight)
			break;

		stack.push(curt);
	}
	
	curt = lex->next_token();

	return true;
}

b8 backtick(Lake* lake, Lexer* lex, TokenStack& stack)
{
	using enum tok;

	stack.push(identifier_lake);
	stack.push(punctuation_dot);
	stack.push(identifier_cmd);
	stack.push(punctuation_lparen);

	Token t = lex->next_token();

	if (t.kind == Whitespace)
	{
		t = lex->next_token();
		if (t.kind == Backtick)
		{
			warn(lake->path, t.column, t.line, "empty cmd!");
			return true;
		}
	}

	for (;;)
	{
		switch (t.kind)
		{
			case Dollar: {
				if (!backtick_dollar(lake, lex, stack, t))
					return false;
			} break;
			case Whitespace: {
				
			} break;
			default: {
				Token save = t;
				backtick_consume_arg(lake, lex, stack, t);
				stack.push({String, {save.raw.s, (s32)(t.raw.s - save.raw.s)}, 0, 0});
			} break;
			case Backtick:;
		}

		if (t.kind == Backtick)
			break;

		stack.push(punctuation_comma);

		t = lex->next_token();
	}
	
	if (stack.stack[stack.len-1].kind == Comma)
	{
		stack.pop(); // idk kinda silly
	}

	stack.push(punctuation_rparen);

	return true;
}

// probably a whitespace delimited string array $[...]
void dollar(Lake* lake, Lexer* lex, TokenStack& stack)
{
	using enum tok;

	Token t = lex->next_token();

	if (t.kind == Whitespace)
		t = lex->next_token();

	if (t.kind != SquareLeft)
	{
		error(lake->path, t.line, t.column, "expected an opening square '[' after '$' to denote whitespace delimited array.");
		exit(1);
	}

	stack.push(punctuation_lbrace);

	for (;;)
	{
		t = lex->next_token();

		if (t.kind == Whitespace)
		{
			stack.push(t);
			continue;
		}
		
		if (t.kind == SquareRight)
			break;
		
		Token save = t;

		for (;;)
		{
			t = lex->next_token();
			if (t.kind == Whitespace)
				break;
			
			if (t.kind == SquareRight)
				break;
		}

		stack.push({String, {save.raw.s, (s32)(t.raw.s - save.raw.s)}, 0, 0});
		stack.push(punctuation_comma);

		if (t.kind == Whitespace)
			stack.push(t);

		if (t.kind == SquareRight)
			break;
	}
	
	stack.push(punctuation_rbrace);
}

void field_list(Lake* lake, Lexer* lexer, Token& curt)
{
	
}

void table_constructor(Lake* lake, Lexer* lexer, Token& curt)
{
	
}

void prefix_expr(Lake* lake, Lexer* lexer, Token& curt)
{
	
}

// consume a lua function
void function(Lake* lake, Lexer* lexer, Token& curt)
{

}

void expression(Lake* lake, Lexer* lex, Token& curt)
{
	using enum tok;

	curt = lex->next_token();

	switch (curt.kind)
	{
		case Nil:
		case False:
		case True: 
		case Number:
		case String:
		case Ellipses:
			return;
		case Function:
			function(lake, lex, curt);
		break;

	}
}

void Lake::run()
{
	using enum tok;

	auto stack = TokenStack::create(); // TODO(sushi) clean up via defer

	for (;;)
	{
		Token t = lex->next_token();

// used when we do a lookahead and have already 
// gotten the next token
skip_next_token:

		switch (t.kind)
		{
			case ColonEqual: {
				if (!stack.insert_before_last_identifier(keyword_local))
				{
					error(path, t.line, t.column, "encountered ':=' but could not find preceeding identifer to place 'local'.");
					exit(1);
				}
				stack.push(punctuation_equal);
			} break;

			case Backtick: {
				backtick(this, lex, stack);
			} break;

			case DotDoubleEqual: {
				Token last_id = stack.get_last_identifier();

				if (last_id.kind == Eof)
				{
					error(path, t.line, t.column, "couldn't find identifier preceeding '..='.\n");
					exit(1);
				}


			} break;

			case QuestionMarkEqual: {
				if (!stack.insert_before_last_identifier(keyword_local))
				{
					error(path, t.line, t.column, "encountered '?=' but could not find preceeding identifier to place 'local'.");
					exit(1);
				}

				// TODO(sushi) this needs to handle entire lua expressions but for now 
				//             its just going to handle single tokens

				Token expr = lex->next_token();
				
				if (expr.kind == Whitespace)
					expr = lex->next_token();

				Token last_id = stack.get_last_identifier();

				stack.push(punctuation_equal);
				stack.push({Whitespace, strl(" "), 0, 0});
				stack.push(identifier_lake);
				stack.push(punctuation_dot);
				stack.push(identifier_declare_if_not_already);
				stack.push(punctuation_lparen);
				stack.push(last_id);
				stack.push(punctuation_comma);
				stack.push(expr);
				stack.push(punctuation_rparen);
			} break;

			case Dollar: {
				dollar(this, lex, stack);
			} break;

			default: {
				stack.push(t);
			} break;
		}

		if (t.kind == tok::Eof) break;

	}

	for (s32 i = 0; i < stack.len; i++)
	{
		Token t = stack.stack[i];
		if (t.kind == String)
			printv("\"", t.raw, "\"");
		else
			print(stack.stack[i].raw);
	}
}

*/
