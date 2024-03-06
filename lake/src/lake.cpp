#include "lake.h"
#include "platform.h"

#include "stdlib.h"
#include "assert.h"

Lake lake;

void Lake::init(str p)
{
	path = p;
	lex = (Lexer*)mem.allocate(sizeof(Lexer));
	buffer = platform::read_file(path);
	lex->init(buffer);
	lex->lake = this;
}

#include "generated/token.strings.h"

const Token identifier_lake = {tok::Identifier, strl("lake"), 0, 0};
const Token identifier_cmd  = {tok::Identifier, strl("cmd"), 0, 0};
const Token identifier_declare_if_not_already = {tok::Identifier, strl("declare_if_not_already"), 0, 0};

const Token punctuation_dot    = {tok::Dot, strl("."), 0, 0};
const Token punctuation_lparen = {tok::ParenLeft, strl("("), 0 ,0};
const Token punctuation_rparen = {tok::ParenRight, strl(")"), 0, 0};
const Token punctuation_comma  = {tok::Comma, strl(","), 0, 0};
const Token punctuation_equal  = {tok::Equal, strl("="), 0, 0 };
const Token punctuation_lbrace = {tok::BraceLeft, strl("{"), 0, 0};
const Token punctuation_rbrace = {tok::BraceRight, strl("}"), 0, 0};

const Token keyword_local = {tok::Local, strl("local "), 0, 0};

struct TokenStack
{
	Token* stack = 0;
	s32 space = 32;
	s32 len = 0;


	static TokenStack create()
	{
		TokenStack out;
		out.stack = (Token*)mem.allocate(sizeof(Token) * out.space);
		return out;
	}

	void destroy()
	{
		mem.free(stack);
	}

	void grow_if_needed(const s32 new_elements)
	{
		if (len + new_elements <= space)
			return;

		while (len + new_elements > space) 
			space *= 2;

		stack = (Token*)mem.reallocate(stack, sizeof(Token) * space);
	}

	void push(Token t)
	{
		grow_if_needed(1);
		
		stack[len] = t;
		len += 1;
	}

	void insert(s64 idx, Token t)
	{
		assert(idx <= len);
		grow_if_needed(1);
		if (!len) push(t);
		else
		{
			mem.move(stack + idx + 1, stack + idx, sizeof(Token) * (len - idx));
			len += 1;
			stack[idx] = t;
		}
	}

	Token pop()
	{
		len -= 1;
		return stack[len];
	}

	b8 insert_before_last_identifier(Token t)
	{
		Token* search = stack + len - 1;
		for (;;)
		{
			if (search->kind == tok::Identifier)
			{
				insert(search - stack, t);
				return true;
			}
			else if (search == stack) 
				return false;
			search -= 1;
		}
	}

	Token get_last_identifier()
	{
		Token* search = stack + len - 1;
		for (;;)
		{
			if (search->kind == tok::Identifier)
			{
				return *search;
			}
			else if (search == stack)
				return {tok::Eof};
			search -= 1;
		}
	}
};

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
			{
				stack.push(t);
				break;
			}
			
			if (t.kind == SquareRight)
				break;
		}

		stack.push({String, {save.raw.s, (s32)(t.raw.s - save.raw.s)}, 0, 0});
		stack.push(punctuation_comma);

		if (t.kind == SquareRight)
			break;
	}
	
	stack.push(punctuation_rbrace);
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
		{
			printv("\"", t.raw, "\"");
		}
		else
		{
			print(stack.stack[i].raw);
		}

	}
}
