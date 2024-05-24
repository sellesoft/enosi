/*
 *  Lakefile parser that transforms it into a normal lua file.
 *
 *  We could maybe just modify the lua parser instead but I'm currently using luajit so that
 *  is a very scary task from what I've seen of its source code.
 *
 *  Also iirc the normal lua parser is a one-shot thing and getting it to do stuff
 *  similar to what lakefiles can do is apparently very hard (according to stack overflow people).
 *
 *  TODO(sushi) take in a longjmp so we can properly exit the parser on error w/o just closing 
 *              the program.
 */

#ifndef _lake_parser_h
#define _lake_parser_h

#include "iro/common.h"

#include "lexer.h"
#include "lake.h"

#include "stdlib.h"

struct Lake;
struct Lexer;
struct Token;

struct Parser
{
	Lexer lex;

	str sourcename;

	Token curt;

	b8 has_lookahead;
	Token lookahead_token;

	using enum tok;

	b8   init(str sourcename, io::IO* in, mem::Allocator* allocator);
	void destroy();

	b8 run();

	/* --------------------------------------------------------------------------------------------
	 *  Write the final result into the given io.
	 */
	b8 fin(io::IO* io);

	/* --------------------------------------------------------------------------------------------
	 *  Token stack
	 *
	 *  TODO(sushi) a linked list of tokens might be better so that inserts 
	 *              dont need to move as much but we also should never insert 
	 *              too far back in the list so maybe not a big deal
	 */
	struct TokenStack {
		struct Elem
		{
			b8 is_virtual;

			struct Virt { tok kind; str raw; };

			union
			{
				Token real;
				Virt virt;
			};
		};

		Array<Elem> arr;

		void init(mem::Allocator* allocator);
		void destroy();

		void push(Token t);
		void push(Elem::Virt v);
		void insert(s64 idx, Token t);
		void insert(s64 idx, Elem::Virt v);
		void pop();

		template<typename... T>
		void pushv(T... args)
		{
			(push(args), ...);
		}

		Token getLastIdentifier();
		b8    insertBeforeLastIdentifier(Elem::Virt v);
		void  pushBeforeWhitespace(Elem::Virt v);

		void print(Parser* p);
		void printSrc(Parser* p);
	} stack;


private:

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
	void nextToken(b8 push_on_stack = true, b8 push_whitespace = true);

	void lookahead(b8 push_on_stack = true, b8 push_whitespace = true);

	/*  Check that we're at the given token kind
	 */ 
	b8 at(tok k);
	b8 lookaheadAt(tok k);

public:

	/*  Report the error where 'curt' currently is and exit.
	 */
	template<typename... T>
	void errorHere(T... args);

	template<typename... T>
	void warnHere(T... args);
};

#endif
