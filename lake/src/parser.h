/*
 *  Lakefile parser that transforms it into a normal lua file.
 *
 *  We could maybe just modify the lua parser instead but I'm currently using luajit so that
 *  is a very scary task from what I've seen of its source code.
 *
 *  Also iirc the normal lua parser is a one-shot thing and getting it to do stuff
 *  similar to what lakefiles can do is apparently very hard (according to stack overflow people).
 */

#ifndef _lake_parser_h
#define _lake_parser_h

#include "common.h"
#include "lexer.h"
#include "lake.h"

#include "stdlib.h"

struct Lake;
struct Lexer;
struct Token;

struct Parser
{
	Lake* lake;
	Lexer* lex;

	Token curt;

	using enum tok;

	void init(Lake* lake);
	void destroy();

	/* --------------------------------------------------------------------------------------------
	 *  Get the final result as a dstr.
	 */
	dstr fin();

	/* --------------------------------------------------------------------------------------------
	 *  Token stack
	 *
	 *  TODO(sushi) a linked list of tokens might be better so that inserts 
	 *              dont need to move as much but we also should never insert 
	 *              too far back in the list so maybe not a big deal
	 */
	struct TokenStack {
		Token* arr;
		s32    space;
		s32    len;

		void init();
		void destroy();

		void grow_if_needed(const s32 new_elements);
		
		void  push(Token t);
		Token pop();
		void  insert(s64 idx, Token t);

		Token get_last_identifier();
		b8    insert_before_last_identifier(Token t);
		void  push_before_whitespace(Token t);

		void print();
		void print_src();
	} stack;

	/* --------------------------------------------------------------------------------------------
	 *  Parsing
	 *
	 *  This is almost completely modeled after Lua's own parser but interlaced with the syntax 
	 *  sugar lakefiles support.
	 *
	 *  The point of all of this is to consume normal lua code while catching the syntax we 
	 *  sprinkle onto it for lakefiles. All lakefile syntax is translated directly (hopefully)
	 *  to pure lua.
	 *
	 *  I think it'd be nice to pull this out for lpp later on. Though in that case it should 
	 *  definitely be optional as this adds a whole layer of stuff ontop of Lua.. but Lua 
	 *  also supports feeding it chunks via a reader..
	 */
	void start();

	/*  Lua parsing
	 */
	void chunk();
	void block();
	void exprstat();
	b8   statement();
	void last_statement();
	void funcname();
	void varlist();
	void var();
	void namelist();
	void exprlist();
	void exprlist1();
	void assignment();
	void primaryexpr(b8* is_call = 0);
	void prefixexpr();
	void expr();
	void subexpr(s32 limit);
	void simpleexpr();
	void functioncall();
	void args();
	void function();
	void body(Token start);
	void funcargs();
	void parlist();
	void tableconstructor();
	void fieldlist();
	void field();
	void recfield();
	void yindex();

	/* --------------------------------------------------------------------------------------------
	 *  Misc helpers
	 */

	/*  Advance curt to the next token while skipping whitespace.
	 *  In most cases we'll just wanna push the token onto the 
	 *  stack, but in some cases where we are looking for our 
	 *  syntax we defer pushing the token until we know what
	 *  we really want.
	 */
	void next_token(b8 push_on_stack = true, b8 push_whitespace = true);

	/*  Check that we're at the given token kind
	 */ 
	b8 at(tok k);

	/*  Report the error where 'curt' currently is and exit.
	 */
	template<typename... T>
	void error_here(T... args)
	{
		print("** ---- **\n\n");
		stack.print_src();
		print("\n\n** ---- **\n\n");
		error(lake->path, curt.line, curt.column, args...);
		exit(1);
	}

	template<typename... T>
	void warn_here(T... args)
	{
		warn(lake->path, curt.line, curt.column, args...);
	}
};

#endif
