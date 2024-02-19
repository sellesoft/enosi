#include "lex.h"
#include "lpp.h"

#include "stdlib.h"
#include "ctype.h"



/* ------------------------------------------------------------------------------------------------
 *  Hash map definitions for keywords. The actual maps are initialized in lpp_lex_init()
 */
#define s(x) {(u8*)u8##x, sizeof(u8##x)}

typedef struct { str s; TokenKind kind; } kwmap_pair;

const kwmap_pair kwmap_def_c[] = 
{
	{s("auto"),     tok_auto},
	{s("break"),    tok_break},
	{s("case"),     tok_case},
	{s("char"),     tok_char},
	{s("const"),    tok_const},
	{s("continue"), tok_continue},
	{s("default"),  tok_default},
	{s("do"),       tok_do},
	{s("double"),   tok_double},
	{s("else"),     tok_else},
	{s("enum"),     tok_enum},
	{s("extern"),   tok_extern},
	{s("float"),    tok_float},
	{s("for"),      tok_for},
	{s("goto"),     tok_goto},
	{s("if"),       tok_if},
	{s("inline"),   tok_inline},
	{s("int"),      tok_int},
	{s("long"),     tok_long},
	{s("register"), tok_register},
	{s("restrict"), tok_restrict},
	{s("return"),   tok_return},
	{s("short"),    tok_short},
	{s("signed"),   tok_signed},
	{s("sizeof"),   tok_sizeof},
	{s("static"),   tok_static},
	{s("struct"),   tok_struct},
	{s("switch"),   tok_switch},
	{s("typedef"),  tok_typedef},
	{s("union"),    tok_union},
	{s("unsigned"), tok_unsigned},
	{s("void"),     tok_void},
	{s("volatile"), tok_volatile},
	{s("while"),    tok_while},
};
const s32 kwmap_def_c_size = sizeof(kwmap_def_c) / sizeof(kwmap_def_c[0]);

const kwmap_pair kwmap_def_cpp[] = 
{
	{s("alignas"),          tok_alignas},
	{s("alignof"),          tok_alignof},
	{s("bool"),             tok_bool},
	{s("catch"),            tok_catch},
	{s("class"),            tok_class},
	{s("concept"),          tok_concept},
	{s("const_cast"),       tok_const_cast},
	{s("consteval"),        tok_consteval},
	{s("constexpr"),        tok_constexpr},
	{s("co_await"),         tok_co_await},
	{s("co_return"),        tok_co_return},
	{s("co_yield"),         tok_co_yield},
	{s("decltype"),         tok_decltype},
	{s("delete"),           tok_delete},
	{s("dynamic_cast"),     tok_dynamic_cast},
	{s("explicit"),         tok_explicit},
	{s("export"),           tok_export},
	{s("false"),            tok_false},
	{s("friend"),           tok_friend},
	{s("mutable"),          tok_mutable},
	{s("namespace"),        tok_namespace},
	{s("new"),              tok_new},
	{s("noexcept"),         tok_noexcept},
	{s("nullptr"),          tok_nullptr},
	{s("operator"),         tok_operator},
	{s("private"),          tok_private},
	{s("protected"),        tok_protected},
	{s("public"),           tok_public},
	{s("reinterpret_cast"), tok_reinterpret_cast},
	{s("requires"),         tok_requires},
	{s("static_assert"),    tok_static_assert},
	{s("static_cast"),      tok_static_cast},
	{s("template"),         tok_template},
	{s("this"),             tok_this},
	{s("thread_local"),     tok_thread_local},
	{s("throw"),            tok_throw},
	{s("true"),             tok_true},
	{s("try"),              tok_try},
	{s("typeid"),           tok_typeid},
	{s("typename"),         tok_typename},
	{s("using"),            tok_using},
	{s("virtual"),          tok_virtual},
	{s("wchar_t"),          tok_wchar_t},
};
const s32 kwmap_def_cpp_size = sizeof(kwmap_def_cpp) / sizeof(kwmap_def_cpp[0]);

typedef struct { u64 key; TokenKind kind; } kwmap_element;

kwmap_element kwmap_c[kwmap_def_c_size];
kwmap_element kwmap_cpp[kwmap_def_cpp_size];

b8 maps_initialized = 0;



typedef struct { u32 idx; b8 found; } kwmap_search_result;

kwmap_search_result search_hash_kwmap_c(u64 hash)
{
	kwmap_search_result result = {};
	s32 l = 0, m = 0, r = kwmap_def_c_size - 1;
	while (l <= r)
	{
		m = l+(r-l)/2;
skip_middle_set:;
		u64 key = kwmap_c[m].key;
		if (key == hash)
		{
			result.idx = m;
			result.found = 1;
			return result;
		}
		else if (key < hash)
		{
			l = m+1;
			if (l>r) break;
			m = l+(r-l)/2;
			goto skip_middle_set;
		} 
		else 
		{
			r = m-1;
		}
	}
	result.idx = m;
	result.found = 0;
	return result;
}

kwmap_search_result search_hash_kwmap_cpp(u64 hash)
{
	kwmap_search_result result = {};
	s32 l = 0, m = 0, r = kwmap_def_cpp_size - 1;
	while (l <= r)
	{
		m = l+(r-l)/2;
skip_middle_set:;
		u64 key = kwmap_cpp[m].key;
		if (key == hash)
		{
			result.idx = m;
			result.found = 1;
			return result;
		}
		else if (key < hash)
		{
			l = m+1;
			if (l>r) break;
			m = l+(r-l)/2;
			goto skip_middle_set;
		} 
		else 
		{
			r = m-1;
		}
	}
	result.idx = m;
	result.found = 0;
	return result;
}

kwmap_search_result search_str_kwmap_c(str s) { return search_hash_kwmap_c(hash_string(s)); }
kwmap_search_result search_str_kwmap_cpp(str s) { return search_hash_kwmap_cpp(hash_string(s)); }

int compare_kwmap_element(const void* l, const void* r)
{
	const kwmap_element* x = l;
	const kwmap_element* y = r;
	return (x->key > y->key) - (x->key < y->key);
}

/* ------------------------------------------------------------------------------------------------
 *  Initializes the hash maps.
 */
void lpp_lex_init()
{
	for (u32 i = 0; i < kwmap_def_c_size; i++)
	{
		kwmap_c[i].key = hash_string(kwmap_def_c[i].s);
		kwmap_c[i].kind = kwmap_def_c[i].kind;
	}
	qsort(kwmap_c, kwmap_def_c_size, sizeof(kwmap_element), compare_kwmap_element);

	for (u32 i = 0; i < kwmap_def_cpp_size; i++)
	{
		kwmap_cpp[i].key = hash_string(kwmap_def_cpp[i].s);
		kwmap_cpp[i].kind = kwmap_def_cpp[i].kind;
	}
	qsort(kwmap_cpp, kwmap_def_cpp_size, sizeof(kwmap_element), compare_kwmap_element);
}


/* ------------------------------------------------------------------------------------------------
 *  Helpers for getting a character relative to the current position in the stream.
 */
static u8 current(Lexer* l) { return *l->stream; }
static u8 next(Lexer* l) { return *(l->stream + 1); }
static u8 nextnext(Lexer* l) { return *(l->stream + 2); }


/* ------------------------------------------------------------------------------------------------
 *  Helpers for querying what the character is at the current position in the stream.
 */
static b8 eof(Lexer* l) { return current(l) == 0; }
static b8 at(Lexer* l, u8 c) { return current(l) == c; }
static b8 next_at(Lexer* l, u8 c) { return next(l) == c; }
static b8 at_identifier_char(Lexer* l) { return isalnum(current(l)) || current(l) == '_'; }

/* ------------------------------------------------------------------------------------------------
 *  Central function for advancing the stream. Keeps track of line and column.
 */
static void advance(Lexer* l)
{
	if (at(l, '\n'))
	{
		l->line += 1;
		l->column = 1;
	}
	else
		l->column += 1;
	l->stream += 1;
}

static void advance2(Lexer* l)
{
	advance(l);
	advance(l);
}

static void advance3(Lexer* l)
{
	advance(l);
	advance(l);
	advance(l);
}

/* ------------------------------------------------------------------------------------------------
 *  Logic for pushing tokens and such
 */
static void grow_tokens_if_needed(Lexer* l)
{
	if (l->token_count + 1 >= l->token_space)
	{
		l->token_space *= 2;
		l->tokens = memory_reallocate(l->tokens, sizeof(Token) * l->token_space);
	}

	if (l->lua_token_count + 1 >= l->lua_token_space)
	{
		l->lua_token_space *= 2;
		l->lua_tokens = memory_reallocate(l->lua_tokens, sizeof(u64) * l->lua_token_space);
	}
}

static void push_token(Lexer* l, Token t)
{
	grow_tokens_if_needed(l);
	l->tokens[l->token_count] = t;
	switch (t.kind)
	{
		case tok_lpp_lua_line:
		case tok_lpp_lua_block:
		case tok_lpp_lua_inline:
		case tok_lpp_lua_macro: 
		case tok_lpp_lua_macro_argument:{
			l->lua_tokens[l->lua_token_count] = l->token_count;
			l->lua_token_count += 1;
		} break;
	}
	l->token_count += 1;
}

static void reset_token(Lexer* l, Token* t)
{
	t->raw.s  = l->stream;
	t->line   = l->line;
	t->column = l->column;
}

/* ------------------------------------------------------------------------------------------------
 *  Helpers for advancing the stream.
 */
static void skip_line(Lexer* l)
{
	while (!at(l, '\n') && !eof(l)) advance(l);
	if (!eof(l)) advance(l);
}

static void skip_whitespace(Lexer* l, b8 make_token)
{
	Token t;
	t.kind = tok_whitespace;
	t.raw.s = l->stream;
	t.line = l->line;
	t.column = l->column;
	while(isspace(current(l))) advance(l);
	t.raw.len = l->stream - t.raw.s;
	if (make_token) push_token(l, t);
}

/* ------------------------------------------------------------------------------------------------
 *
 */
void lpp_lexer_init(lppContext* lpp, Lexer* l, u8* stream)
{
	if (!maps_initialized) lpp_lex_init();

	l->lpp = lpp;
	l->token_count = l->lua_token_count = 0;
	l->token_space = l->lua_token_space = 4;
	l->tokens = memory_reallocate(0, sizeof(Token) * l->token_space);
	l->lua_tokens = memory_reallocate(0, sizeof(u64) * l->lua_token_space);
	l->stream = stream;
}

/* ------------------------------------------------------------------------------------------------
 *
 */
void lpp_lexer_run(Lexer* l)
{
	lppContext* lpp = l->lpp;

	Token t;
	
#define one_char_token(x, _kind) \
		case x: {                \
			t.kind = _kind;      \
			t.raw.len = 1;       \
			advance(l);          \
		} break;      
#define one_or_two_char_token(x0, kind0, x1, kind1) \
		case x0: {                                  \
			if (next_at(l, x1)) {                   \
				t.kind = kind1;                     \
				t.raw.len = 2;                      \
				advance2(l);                        \
			}                                       \
			else {                                  \
				t.kind = kind0;                     \
				t.raw.len = 1;                      \
				advance(l);                         \
			}                                       \
		} break;                                    
#define one_or_either_of_two_chars_token(x0, kind0, x1, kind1, x2, kind2) \
		case x0: {                                                        \
			switch (next(l)) {                                            \
				case x1: {                                                \
					t.kind = kind1;                                       \
					t.raw.len = 2;                                        \
					advance2(l);                                          \
				} break;                                                  \
				case x2: {                                                \
					t.kind = kind2;                                       \
					t.raw.len = 2;                                        \
					advance2(l);                                          \
				} break;                                                  \
				default: {                                                \
					t.kind = kind0;                                       \
					t.raw.len = 1;                                        \
					advance(l);                                           \
				} break;                                                  \
			}                                                             \
		} break;
#define one_two_or_three_chars_token(x0, kind0, x1, kind1, x2, kind2, x3, kind3) \
		case x0: {                                                               \
			switch (next(l)) {                                                   \
				case x1: {                                                       \
					t.raw.len = 2;                                               \
					t.kind = kind1;                                              \
					advance2(l);                                                 \
				} break;                                                         \
				case x2: {                                                       \
					if (nextnext(l) == x3) {                                     \
						t.raw.len = 3;                                           \
						t.kind = kind3;                                          \
						advance3(l);                                             \
					} else {                                                     \
						t.raw.len = 2;                                           \
						t.kind = kind2;                                          \
						advance2(l);                                             \
					}                                                            \
				} break;                                                         \
				default: {                                                       \
					t.kind = kind0;                                              \
					t.raw.len = 1;                                               \
					advance(l);                                                  \
				} break;                                                         \
			}                                                                    \
		} break;                    


	skip_whitespace(l, 1);
	while (!eof(l))
	{
		reset_token(l, &t);

		switch (current(l))
		{
			one_char_token('{', tok_brace_left);
			one_char_token('}', tok_brace_right);
			one_char_token('(', tok_parenthesis_left);
			one_char_token(')', tok_parenthesis_right);
			one_char_token('[', tok_square_left);
			one_char_token(']', tok_square_right);
			one_char_token(',', tok_comma);
			one_char_token('?', tok_question_mark);
			one_char_token('#', tok_pound);
			one_char_token(':', tok_colon);
			one_char_token(';', tok_semicolon);
			one_char_token('=', tok_equal);
			one_char_token('.', tok_period);

			one_or_two_char_token('+', tok_plus,             '=', tok_plus_equal);
			one_or_two_char_token('*', tok_asterisk,         '=', tok_asterisk_equal);
			one_or_two_char_token('%', tok_percent,          '=', tok_percent_equal);
			one_or_two_char_token('~', tok_tilde,            '=', tok_tilde_equal);
			one_or_two_char_token('^', tok_caret,            '=', tok_caret_equal);
			one_or_two_char_token('!', tok_explanation_mark, '=', tok_explanation_mark_equal);

			one_or_either_of_two_chars_token('-', tok_minus,     '=', tok_minus_equal,     '>', tok_arrow);
			one_or_either_of_two_chars_token('&', tok_ampersand, '=', tok_ampersand_equal, '&', tok_ampersand_double);

			one_two_or_three_chars_token('<', tok_angle_left,  '=', tok_angle_left_equal,  '<', tok_angle_left_double,  '=', tok_angle_left_double_equal);
			one_two_or_three_chars_token('>', tok_angle_right, '=', tok_angle_right_equal, '>', tok_angle_right_double, '=', tok_angle_right_double_equal);
			
			case '/': {
				if (next_at(l, '/'))
				{
					skip_line(l);
					t.kind = tok_comment;
					t.raw.len = l->stream - t.raw.s;
				}
				else if (next_at(l, '*'))
				{
					s64 line = l->line;
					s64 column = l->column;
					while ((!at(l, '*') || !next_at(l, '/')) && !eof(l)) advance(l);
					if (eof(l))
						fatal_error(lpp, l->line, l->column, "encountered end of file while consuming C block comment that started at %li:%li\n", line, column);
					t.kind = tok_comment;
					t.raw.len = l->stream - t.raw.s;
				}
				else if (next_at(l, '='))
				{
					t.raw.len = 2;
					t.kind = tok_slash_equal;
					advance2(l);
				}
				else
				{
					t.raw.len = 1;
					t.kind = tok_slash;
					advance(l);
				}
			} break;

			case '"': {
				s64 line = l->line;
				s64 column = l->column;
				advance(l);
				while (!at(l, '"') && !eof(l)) advance(l);
				if (eof(l))
					fatal_error(lpp, line, column, "unexpected end of file while consuming string literal\n");
				t.kind = tok_literal_string;
				t.raw.s += 1;
				t.raw.len = l->stream - t.raw.s;
				advance(l);
			} break;

			default: {
				// handle numbers, identifiers, and keywords
				
				if (isdigit(current(l)))
				{
					while (isdigit(current(l))) advance(l);

					switch (current(l))
					{
						case 'e':
						case 'E':
						case '.': {
							advance(l);
							while(isdigit(current(l))) advance(l);
							if (at(l, 'f')) 
							{
								t.kind = tok_literal_float;
								t.literal._f32 = strtof((char*)t.raw.s, 0); // TODO(sushi) replace with impl that takes a length
							}
							else 
							{
								t.kind = tok_literal_double;
								t.literal._f64 = strtod((char*)t.raw.s, 0); // TODO(sushi) replace with impl that takes a length
							}
							t.raw.len = l->stream - t.raw.s;
						} break;

						case 'x':
						case 'X':{
							advance(l);
							while(isdigit(current(l))) advance(l);
							t.kind = tok_literal_integer;
							t.raw.len = l->stream - t.raw.s;
							t.literal._s64 = strtoll((char*)t.raw.s, 0, 16); // TODO(sushi) replace with impl that takes a length
						} break;

						default: {
							t.raw.len = l->stream - t.raw.s;
							t.kind = tok_literal_integer;
							t.literal._s64 = strtoll((char*)t.raw.s, 0, 10);
						} break;
					}
				}
				else
				{
					// must be either a keyword or identifier
					if (!isalpha(current(l))) 
						fatal_error(lpp, l->line, l->column, "invalid token: '%c'\n", current(l));
					
					advance(l);
					while (at_identifier_char(l) && !eof(l)) advance(l);

					t.raw.len = l->stream - t.raw.s;
					
					u64 hash = hash_string(t.raw);
					kwmap_search_result search = search_hash_kwmap_c(hash);
					if (!search.found)
					{
						if (lpp->cpp)
						{
							search = search_hash_kwmap_cpp(hash);
							if (search.found)
								t.kind = kwmap_cpp[search.idx].kind;
							else
								t.kind = tok_identifier;
						}
						else
							t.kind = tok_identifier;
					}
				}
			} break;

			/* -------------------------------------------
			 *  lpp syntax
			 */
			case '$': {
				advance(l);
				s64 line = l->line;
				s64 column = l->column;
				skip_whitespace(l, 0);
				if (at(l, '{'))
				{
					u64 nesting = 1;
					t.raw.s += l->stream - t.raw.s + 1; // move past the { 
					advance(l);
					while (!eof(l))
					{	
						advance(l);
						if (at(l, '}'))
						{
							nesting -= 1;
							if (!nesting) break;
						}
						else if(at(l, '{'))
							nesting += 1;
					}
					if (eof(l))
						fatal_error(lpp, line, column, "unexpected end of file while consuming lua block\n");
                    t.raw.len = l->stream - t.raw.s;
					t.kind = tok_lpp_lua_block;
					advance(l);
				}
				else
				{
					if (line != l->line)
						warning(lpp, line, column, "empty lua line");
					else
						skip_line(l);
					t.raw.s += 1;
					t.raw.len = l->stream - t.raw.s;
					t.kind = tok_lpp_lua_line;
				}
			} break;

			case '`': {
				s64 line = l->line;
				s64 column = l->column;
				advance(l);
				while (!at(l, '`') && !eof(l)) advance(l);
				if (eof(l))
					fatal_error(lpp, line, column, "unexpected end of file while consuming inline lua\n");
				t.raw.s += 1; // move away from the backtick
				t.raw.len = l->stream - t.raw.s;
				t.kind = tok_lpp_lua_inline;
				advance(l);
			} break;

			case '@': {
				s64 line = l->line;
				s64 column = l->column;
				advance(l);
				skip_whitespace(l, 0);
				if (!at_identifier_char(l))
					fatal_error(lpp, l->line, l->column, "token following a lua macro (@) must be an identifier of a lua function\n");
				while (at_identifier_char(l) && !eof(l)) advance(l);
				if (eof(l))
					fatal_error(lpp, l->line, l->column, "unexpected end of file while consuming lua macro\n");

				// finish the macro token
				t.kind = tok_lpp_lua_macro;
				t.raw.s += 1;
				t.raw.len = l->stream - t.raw.s;
				push_token(l, t);

				skip_whitespace(l, 0);

				if (at(l, '('))
				{
					advance(l);
					// turn each argument into a arg token
					while (!eof(l))
					{	
						skip_whitespace(l, 0);
						reset_token(l, &t);
						while (!at(l, ',') && !at(l, ')') && !eof(l)) advance(l);
						if (eof(l))
							fatal_error(lpp, line, column, "unexpected end of file while consuming lua macro arguments\n");
						t.raw.len = l->stream - t.raw.s;
						t.kind = tok_lpp_lua_macro_argument;
						b8 quit = at(l, ')');
						advance(l);
						if (quit)
							break;
						else
							push_token(l, t);
					}
				}
				else if (at(l, '"'))
				{
					advance(l);
					reset_token(l, &t);
					while(!at(l, '"') && !eof(l)) advance(l);
					if (eof(l))
						fatal_error(lpp, line, column, "unexpected end of file while consume lua string macro arg\n");

					t.raw.len = l->stream - t.raw.s;
					t.kind = tok_lpp_lua_macro_argument;
					advance(l);
				}
			} break;
		}
		push_token(l, t);
		skip_whitespace(l, 1);
	}
}

