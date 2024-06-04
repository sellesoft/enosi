/*
 *  Extension for lpp that interfaces to the clang api for semantic manipulation of 
 *  C/C++ code via lpp.
 */

#ifndef _lppclang_lppclang_h
#define _lppclang_lppclang_h

#include "iro/common.h"
#include "iro/unicode.h"

using namespace iro;

//!ffiapi start

typedef enum
{
	DeclKind_Invalid,

	// Not handled yet by this api
	DeclKind_Unknown,

	// A top level function
	DeclKind_Function,

	// A function on a structure
	DeclKind_Method,

	// A parameter of a method or function
	DeclKind_Parameter,

	// A local or global variable
	DeclKind_Variable,

	// A variable member of a structure
	DeclKind_Field,

	DeclKind_Typedef,

	// struct/union/class
	DeclKind_Record,

	DeclKind_Enum,
} DeclKind;

//!ffiapi end

static const str declkind_strs[] =
{
	"Invalid"_str,
	"Unknown"_str,
	"Function"_str,
	"Method"_str,
	"Parameter"_str,
	"Variable"_str,
	"Field"_str,
	"Typedef"_str,
	"Record"_str,
	"Enum"_str,
};

//!ffiapi start

typedef enum
{
	// NOTE(sushi) we only really care about tokens that could take on any form.
	//             all others (punc, keywords, etc.) can be handled by string 
	//             comparing in lua.
	TK_EndOfFile,

	TK_Unhandled,

	TK_Comment,
	TK_Whitespace,

	TK_Identifier,

	TK_NumericConstant,
	TK_CharConstant,
	TK_StringLiteral,
} TokenKind;

//!ffiapi end

extern "C"
{

//!ffiapi start

typedef struct Lexer Lexer;
typedef struct Token Token;
typedef struct Context Context;
typedef struct Decl Decl;
typedef struct Type Type;
typedef struct DeclIter DeclIter;
typedef struct ParamIter ParamIter;
typedef struct FieldIter FieldIter;
typedef struct EnumIter EnumIter;

Decl* testIncParse(str s);

/* ------------------------------------------------------------------------------------------------ create/destroyContext
 |  Create/destroy an lppclang context. This keeps track of clang's ASTUnit and also various things
 |  that we must dynamically allocate.
 */
Context* createContext();
void     destroyContext(Context*);

/* ------------------------------------------------------------------------------------------------ createLexer
 |  Create a lexer over the given string.
 */
Lexer* createLexer(Context* ctx, str s);

/* ------------------------------------------------------------------------------------------------ lexerNextToken
 |  Returns a pointer to the next token in the string or nullptr if none are left.
 */
Token* lexerNextToken(Lexer* l);

/* ------------------------------------------------------------------------------------------------ tokenGetRaw
 |  Returns a str representing the raw text the given token spans.
 */ 
str tokenGetRaw(Context* ctx, Lexer* l, Token* t);

/* ------------------------------------------------------------------------------------------------ tokenGetKind
 |  Get the kind of a token.
 */
TokenKind tokenGetKind(Token* t);

/* ------------------------------------------------------------------------------------------------ createASTFromString
 |  Create an AST from the given string for the given Context.
 */ 
b8 createASTFromString(Context* ctx, str s);

/* ------------------------------------------------------------------------------------------------ getTranslationUnitDecl
 |  Retrieves the toplevel declaration of the given context.
 */
Decl* getTranslationUnitDecl(Context* ctx);

/* ------------------------------------------------------------------------------------------------ createDeclIter/getNextDecl
 |  Given that 'decl' is also a declaration context, creates and returns an iterator over each of 
 |  its interior declarations. This works for things like structures, namespaces, the translation
 |  unit retrieved via getTranslationUnitDecl(), etc.
 |
 |  If the given decl is not a declaration context or is empty, nullptr is returned.
 */
DeclIter* createDeclIter(Context* ctx, Decl* decl);
Decl*     getNextDecl(DeclIter* iter);

/* ------------------------------------------------------------------------------------------------ getDeclKind
 |  Returns the kind of declaration 'decl' represents, if it is known to this api.
 */
DeclKind getDeclKind(Decl* decl);

/* ------------------------------------------------------------------------------------------------ getDeclName
 |  Assuming 'decl' is a named declaration, retrieve its name
 */ 
str getDeclName(Decl* decl);

/* ------------------------------------------------------------------------------------------------ getDeclType
 |  Assuming 'decl' is a value declaration (eg. a declaration with an underlying type), retrieve 
 |  its type.
 */ 
Type* getDeclType(Decl* decl);

// retrieves the Type representation of a Type declaration.
// This does not give the type of the declaraion!!
Type* getTypeDeclType(Context* ctx, Decl* decl);

/* ------------------------------------------------------------------------------------------------ getDeclBegin/End
 |  Gets the offset into the provided string where the given declaration begins and ends.
 */
u64 getDeclBegin(Context* ctx, Decl* decl);
u64 getDeclEnd(Context* ctx, Decl* decl);

/* ------------------------------------------------------------------------------------------------ getFunctionReturnType
 |  If the given decl is a function declaration, retrieve its return type. Otherwise nullptr
 |  is returned.
 */
Type* getFunctionReturnType(Decl* decl);

/* ------------------------------------------------------------------------------------------------ functionHasBody/getFunctionBodyBegin/End
 |  Helpers for determining if the given function or method has a body and where that body 
 |  begins and ends.
 */
b8 	functionHasBody(Decl* decl);
u32 getFunctionBodyBegin(Decl* decl);
u32 getFunctionBodyEnd(Decl* decl);

/* ------------------------------------------------------------------------------------------------ tagHasBody/getTagBodyBegin/End
 |  Helpers for determining if the given tag declaration (a struct, class, enum, etc.) has a body
 |  and where that body begins and ends.
 */
b8  tagHasBody(Decl* decl);
u32 getTagBodyBegin(Decl* decl);
u32 getTagBodyEnd(Decl* decl);

/* ------------------------------------------------------------------------------------------------ tagIsEmbeddedInDeclarator
 |  Determine if this tag declaration (a struct, class, enum, etc.) is embedded in a declarator.
 |  Eg. is this tag being defined for the first time in the syntax of a declarator:
 |  
 |  struct Apple {...} apple;
 */
b8 tagIsEmbeddedInDeclarator(Decl* decl);

/* ------------------------------------------------------------------------------------------------ createParamIter/getNextParam
 |  Given that decl is a function declaration, create and return an iterator over its parameters
 |  via getNextParam().
 */
ParamIter* createParamIter(Context* ctx, Decl* decl);
Decl*      getNextParam(ParamIter* iter);

/* ------------------------------------------------------------------------------------------------ isCanonical/Unqualified
 |  Helpers for determining if the given type is canonical/unqualified.
 */
b8 isCanonical(Type* type);
b8 isUnqualified(Type* type);
b8 isUnqualifiedAndCanonical(Type* type);

b8 isConst(Type* type);

/* ------------------------------------------------------------------------------------------------ getCanonicalType
 |  Retrieves the canonical type of the given type, eg. the underlying type with syntax sugar 
 |  removed. This will still have type qualifiers (such as 'const').
 */
Type* getCanonicalType(Type* type);

/* ------------------------------------------------------------------------------------------------ getUnqualifiedType
 |  Retrieves the type with qualifiers removed.
 */
Type* getUnqualifiedType(Type* type);

/* ------------------------------------------------------------------------------------------------ getUnqualifiedCanonicalType
 |  Combination of getCanonicalType and getUnqualifiedType.
 */
Type* getUnqualifiedCanonicalType(Type* type);

/* ------------------------------------------------------------------------------------------------ getTypeName
 |  Returns a string representing the name of the given type. What is returned depends on the 
 |  sugar and qualifications present on the type. 
 */
str getTypeName(Context* ctx, Type* type);

/* ------------------------------------------------------------------------------------------------ getCanonicalTypeName
 |  Returns a string containing the 'canonical' spelling of 
 |  the type's name. Eg. no matter the sugar that the given type 
 |  has this will always return the same name for the underlying type. 
 |
 |  For example, if we have 
 |
 |      namespace stuff 
 |      {
 |     
 |      struct Apple {}
 |
 |      }
 |
 |      using MyApple = stuff::Apple;
 |
 |      const MyApple a;
 |
 |  and you use this function to determine the type
 |  name of the type returned by getDeclType() on 'a',
 |  you will get 'const struct stuff::Apple'.
 |
 |  To get the typename as it is written in source code
 |  use getTypeName().
 */
str getCanonicalTypeName(Context* ctx, Type* type);

/* ------------------------------------------------------------------------------------------------ getUnqualifiedCanonicalTypeName
 |  Same as getCanonicalTypeName but with qualifiers removed.
 */
str getUnqualifiedCanonicalTypeName(Context* ctx, Type* type);

// Retrieve the size in BITS of the given type.
u64 getTypeSize(Context* ctx, Type* type);

b8 typeIsBuiltin(Type* type);

// Get the declaration of this type if applicable.
// Returns nullptr if this is not possible, eg. 
// if this is a builtin type.
Decl* getTypeDecl(Type* type);

FieldIter* createFieldIter(Context* ctx, Decl* decl);
Decl*      getNextField(FieldIter* iter);

u64 getFieldOffset(Context* ctx, Decl* field);

EnumIter* createEnumIter(Context* ctx, Decl* decl);
Decl*     getNextEnum(EnumIter* iter);

/* ------------------------------------------------------------------------------------------------ dumpAST
 |  Asks clang to dump the loaded ast to stdout for debug purposes.
 */
void dumpAST(Context* ctx);

/* ------------------------------------------------------------------------------------------------ getClangDeclSpelling
 |  Retrieves clang's internal naming of the given decl for debugging decl kinds unknown to the 
 |  api.
 */
str getClangDeclSpelling(Decl* decl);

//!ffiapi end

}

#endif
