/*
 *  Extension for lpp that interfaces to the clang api for semantic 
 *  manipulation of C/C++ code via lpp.
 */

#ifndef _lppclang_LppClang_h
#define _lppclang_LppClang_h

#include "iro/Common.h"
#include "iro/Unicode.h"

using namespace iro;

// Shorthand so that im not extending the line length of each function
// by so many characters
#define LPPCFUNC EXPORT_DYNAMIC

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

static const String declkind_strs[] =
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
typedef struct Stmt Stmt;
typedef struct Expr Expr;
typedef struct TemplateArg TemplateArg;
typedef struct TemplateArgIter TemplateArgIter;
typedef struct BaseIter BaseIter;

/* ============================================================================
 */
typedef struct ParseStmtResult
{
  Stmt* stmt;
  // Offset into the buffer where parsing stopped.
  // -1 if EOF.
  s64 offset;
} ParseStmtResult;

/* ============================================================================
 */
typedef struct ParseDeclResult
{
  Decl* decl;
  // Offset into the buffer where parsing stopped.
  // -1 if EOF.
  s64 offset;
} ParseDeclResult;

/* ============================================================================
 */
typedef struct ParseTopLevelDeclsResult
{
  DeclIter* iter;
  // Offset into the buffer where parsing stopped.
  // -1 if EOF.
  s64 offset;
} ParseTopLevelDeclsResult;

/* ============================================================================
 */
typedef struct ParseTypeNameResult
{
  Type* type;
  // Offset into the buffer where parsing stopped.
  // -1 if EOF.
  s64 offset;
} ParseTypeNameResult;

/* ============================================================================
 */
typedef struct ParseExprResult
{
  Expr* expr;
  // Offset into the buffer where parsing stopped.
  // -1 if EOF.
  s64 offset;
} ParseExprResult;

/* ============================================================================
 */
typedef struct ParseIdentifierResult
{
  String id;
  s64 offset;
} ParseIdentifierResult;

/* ============================================================================
 */
typedef struct
{
  String raw;
  u64 start_offset;
  u64 end_offset;
} TokenRawAndLoc;

// **** EXPERIMENTAL ****
LPPCFUNC void addIncludeDir(Context* ctx, String s);
LPPCFUNC Type* lookupType(Context* ctx, String name);
LPPCFUNC ParseStmtResult parseStatement(Context* ctx);
// Returns the declaration of the given statement, or nullptr if the given
// stmt is not a declaration.
LPPCFUNC Decl* getStmtDecl(Context* ctx, Stmt* stmt);
LPPCFUNC ParseTypeNameResult parseTypeName(Context* ctx);
LPPCFUNC ParseDeclResult parseTopLevelDecl(Context* ctx);
LPPCFUNC ParseTopLevelDeclsResult parseTopLevelDecls(Context* ctx);
LPPCFUNC ParseExprResult parseExpr(Context* ctx);
LPPCFUNC ParseIdentifierResult parseIdentifier(Context* ctx);
LPPCFUNC Decl* lookupName(Context* ctx, String s);
LPPCFUNC b8 loadString(Context* ctx, String s);
LPPCFUNC String getDependencies(
    String filename, String file, String* args, u64 argc);
LPPCFUNC void destroyDependencies(String deps);
LPPCFUNC b8 beginNamespace(Context* ctx, String name);
LPPCFUNC void endNamespace(Context* ctx);
// **** EXPERIMENTAL ****

/* ----------------------------------------------------------------------------
 |  Create/destroy an lppclang context. This keeps track of clang's ASTUnit and 
 |  also various things that we must dynamically allocate.
 */

LPPCFUNC Context* createContext(String* args, u64 argc);
LPPCFUNC void     destroyContext(Context*);

/* ----------------------------------------------------------------------------
 |  Create a lexer over the given string.
 */
LPPCFUNC Lexer* createLexer(Context* ctx, String s);

/* ----------------------------------------------------------------------------
 |  Returns a pointer to the next token in the string or nullptr if none are 
 |  left.
 */
LPPCFUNC Token* lexerNextToken(Lexer* l);

/* ----------------------------------------------------------------------------
 |  Returns a String representing the raw text the given token spans.
 */ 
LPPCFUNC String tokenGetRaw(Context* ctx, Lexer* l, Token* t);

/* ----------------------------------------------------------------------------
 |  Get the kind of a token.
 */
LPPCFUNC TokenKind tokenGetKind(Token* t);

/* ----------------------------------------------------------------------------
 |  Create an AST from the given string for the given Context.
 */ 
LPPCFUNC b8 createASTFromString(Context* ctx, String s);

/* ----------------------------------------------------------------------------
 */
LPPCFUNC Decl* parseString(Context* ctx, String s);

/* ----------------------------------------------------------------------------
 |  Prints clang's textual representation of the given decl to stdout
 */ 
LPPCFUNC void dumpDecl(Decl* decl);

/* ----------------------------------------------------------------------------
 |  Retrieves the toplevel declaration of the given context.
 */
LPPCFUNC Decl* getTranslationUnitDecl(Context* ctx);

/* ----------------------------------------------------------------------------
 |  Given that 'decl' is also a declaration context, creates and returns an 
 |  iterator over each of its interior declarations. This works for things 
 |  like structures, namespaces, the translation unit retrieved via 
 |  getTranslationUnitDecl(), etc.
 |
 |  If the given decl is not a declaration context or is empty, nullptr is 
 |  returned.
 */
LPPCFUNC DeclIter* createDeclIter(Context* ctx, Decl* decl);
LPPCFUNC Decl*     getNextDecl(DeclIter* iter);

/* ----------------------------------------------------------------------------
 |  Returns an iterator over the declaration context that the given Decl
 |  belongs to. This starts at the beginning of the context!
 */
LPPCFUNC DeclIter* getContextOfDecl(Context* ctx, Decl* decl);

/* ----------------------------------------------------------------------------
 |  Returns the kind of declaration 'decl' represents, if it is known to this 
 |  api.
 */
LPPCFUNC DeclKind getDeclKind(Decl* decl);

/* ----------------------------------------------------------------------------
 |  Assuming 'decl' is a named declaration, retrieve its name
 */ 
LPPCFUNC String getDeclName(Decl* decl);

/* ----------------------------------------------------------------------------
 |  Assuming 'decl' is a value declaration (eg. a declaration with an 
 |  underlying type), retrieve its type.
 */ 
LPPCFUNC Type* getDeclType(Decl* decl);

/* ----------------------------------------------------------------------------
 |  Gets the offset into the provided string where the given declaration 
 |  begins and ends.
 */
LPPCFUNC u64 getDeclBegin(Context* ctx, Decl* decl);
LPPCFUNC u64 getDeclEnd(Context* ctx, Decl* decl);

/* ----------------------------------------------------------------------------
 *  Whether this declares a type.
 */
LPPCFUNC b8 isTypeDecl(Decl* decl);

/* ----------------------------------------------------------------------------
 *  Gets the Type of a type declaration.
 */
LPPCFUNC Type* getTypeDeclType(Context* ctx, Decl* decl);

/* ----------------------------------------------------------------------------
 *  Whether this decl represents a struct or union.
 */
LPPCFUNC b8 isRecord(Decl* decl);

/* ----------------------------------------------------------------------------
 *  Disambiguate between specific types.
 */
LPPCFUNC b8 isStruct(Decl* decl);
LPPCFUNC b8 isUnion(Decl* decl);
LPPCFUNC b8 isEnum(Decl* decl);
LPPCFUNC b8 isTypedef(Decl* decl);

/* ---------------------------------------------------------------------------
 */
LPPCFUNC BaseIter* createBaseIter(Context* ctx, Decl* decl);
LPPCFUNC Type* nextBase(BaseIter* iter);

/* ---------------------------------------------------------------------------
 *  Checks if this is a struct, union, or enum.
 */
LPPCFUNC b8 isTagDecl(Decl* decl);

// NOTE these two only work on structs for now.
LPPCFUNC b8 isTemplate(Decl* decl);
LPPCFUNC b8 isTemplateSpecialization(Decl* decl);
LPPCFUNC Decl* getSpecializedDecl(Decl* decl);

/* ----------------------------------------------------------------------------
 */
LPPCFUNC TemplateArgIter* getTemplateArgIter(Context* ctx, Decl* decl);
LPPCFUNC TemplateArg* getNextTemplateArg(TemplateArgIter* iter);

/* ----------------------------------------------------------------------------
 */
LPPCFUNC b8 isTemplateArgType(TemplateArg* arg);
LPPCFUNC Type* getTemplateArgType(TemplateArg* arg);
LPPCFUNC b8 isTemplateArgIntegral(TemplateArg* arg);
LPPCFUNC s64 getTemplateArgIntegral(TemplateArg* arg);

LPPCFUNC b8 isNamespace(Decl* decl);

// Only works on 'record' types; structs or unions.
LPPCFUNC b8 isAnonymous(Decl* decl);

// Checks if 'decl' is a FieldDecl, eg. a member of a struct or union.
LPPCFUNC b8 isField(Decl* decl);

LPPCFUNC b8 isFunction(Decl* decl);

// When 'decl' is a FieldDecl, check if its anonymous, eg. a nested anonymous 
// struct or union with no field name. 
// 'decl' must be a FieldDecl.
LPPCFUNC b8 isAnonymousField(Decl* decl);

// When 'field' is a FieldDecl, get its offset into whatever its in in bits.
// Asserts that 'field' is a FieldDecl.
LPPCFUNC u64 getFieldOffset(Context* ctx, Decl* field);

/* ----------------------------------------------------------------------------
 */
LPPCFUNC b8 isComplete(Decl* decl);

/* ---------------------------------------------------------------------------
 */
LPPCFUNC Type* getTypedefSubType(Decl* decl);

/* ---------------------------------------------------------------------------
 */
LPPCFUNC Decl* getTypedefSubDecl(Decl* decl);

/* ----------------------------------------------------------------------------
 *  Gets a comment, if any, attached to the given Decl.
 */
LPPCFUNC String getComment(Context* ctx, Decl* decl);

/* ----------------------------------------------------------------------------
 *  Retrieves the declaration that actually defines whatever this is. Eg.
 *  there are Decls for forward declarations that are actually defined later.
 *
 *  This only works on TagDecls, like structs, unions, enums, etc.
 */
LPPCFUNC Decl* getDefinition(Decl* decl);

/* ----------------------------------------------------------------------------
 */
LPPCFUNC void makeComplete(Context* ctx, Type* type);

/* ----------------------------------------------------------------------------
 |  If this is the 'canonical' decl.
 */
LPPCFUNC b8 isCanonicalDecl(Decl* decl);

/* ----------------------------------------------------------------------------
 |  If the given decl is a function declaration, retrieve its return type. 
 |  Otherwise nullptr is returned.
 */
LPPCFUNC Type* getFunctionReturnType(Decl* decl);

/* ----------------------------------------------------------------------------
 |  Helpers for determining if the given function or method has a body and 
 |  where that body begins and ends.
 */
LPPCFUNC b8  functionHasBody(Decl* decl);
LPPCFUNC u32 getFunctionBodyBegin(Decl* decl);
LPPCFUNC u32 getFunctionBodyEnd(Decl* decl);

/* ----------------------------------------------------------------------------
 |  Helpers for determining if the given tag declaration (a struct, class, 
 |  enum, etc.) has a body and where that body begins and ends.
 */
LPPCFUNC b8  tagHasBody(Decl* decl);
LPPCFUNC u32 getTagBodyBegin(Decl* decl);
LPPCFUNC u32 getTagBodyEnd(Decl* decl);

/* ----------------------------------------------------------------------------
 |  Determine if this tag declaration (a struct, class, enum, etc.) is embedded 
 |  in a declarator. Eg. is this tag being defined for the first time in the 
 |  syntax of a declarator:
 |  
 |  struct Apple {...} apple;
 */
LPPCFUNC b8 tagIsEmbeddedInDeclarator(Decl* decl);

/* ----------------------------------------------------------------------------
 |  Given that decl is a function declaration, create and return an iterator 
 |  over its parameters via getNextParam().
 */
LPPCFUNC ParamIter* createParamIter(Context* ctx, Decl* decl);
LPPCFUNC Decl*      getNextParam(ParamIter* iter);

/* ----------------------------------------------------------------------------
 |  Helpers for determining if the given type is canonical/unqualified.
 */
LPPCFUNC b8 isCanonical(Type* type);
LPPCFUNC b8 isUnqualified(Type* type);
LPPCFUNC b8 isUnqualifiedAndCanonical(Type* type);
LPPCFUNC b8 isConst(Type* type);

/* ----------------------------------------------------------------------------
 *  Returns if the given type is 'elaborated' eg. the Clang type represents 
 *  the type as it was written in source.
 */
LPPCFUNC b8 isElaborated(Type* type);

/* ----------------------------------------------------------------------------
 */
LPPCFUNC Type* getDesugaredType(Context* ctx, Type* type);

/* ----------------------------------------------------------------------------
 */
LPPCFUNC Type* getSingleStepDesugaredType(Context* ctx, Type* type);

/* ---------------------------------------------------------------------------
 */
LPPCFUNC b8 isTypedefType(Type* type);

/* ---------------------------------------------------------------------------
 */
LPPCFUNC Decl* getTypedefTypeDecl(Type* type);

/* ----------------------------------------------------------------------------
 |  Helpers for dealing with pointers.
 */
LPPCFUNC b8 isPointer(Type* type);
LPPCFUNC b8 isFunctionPointer(Type* type);
LPPCFUNC b8 isReference(Type* type);
LPPCFUNC Type* getPointeeType(Type* type);

LPPCFUNC b8 isArray(Type* type);
LPPCFUNC Type* getArrayElementType(Context* ctx, Type* type);
LPPCFUNC u64 getArrayLen(Context* ctx, Type* type);

/* ----------------------------------------------------------------------------
 */
LPPCFUNC b8 isTemplateSpecializationType(Type* type);

/* ----------------------------------------------------------------------------
 |  Retrieves the canonical type of the given type, eg. the underlying type 
 |  with syntax sugar removed. This will still have type qualifiers (such as 
 |  'const').
 */
LPPCFUNC Type* getCanonicalType(Type* type);

/* ----------------------------------------------------------------------------
 |  Retrieves the type with qualifiers removed.
 */
LPPCFUNC Type* getUnqualifiedType(Type* type);

/* ----------------------------------------------------------------------------
 |  Combination of getCanonicalType and getUnqualifiedType.
 */
LPPCFUNC Type* getUnqualifiedCanonicalType(Type* type);

/* ----------------------------------------------------------------------------
 |  Returns a string representing the name of the given type. What is returned 
 |  depends on the sugar and qualifications present on the type. 
 */
LPPCFUNC String getTypeName(Context* ctx, Type* type);

/* ----------------------------------------------------------------------------
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
LPPCFUNC String getCanonicalTypeName(Context* ctx, Type* type);

/* ----------------------------------------------------------------------------
 |  Same as getCanonicalTypeName but with qualifiers removed.
 */
LPPCFUNC String getUnqualifiedCanonicalTypeName(Context* ctx, Type* type);

// Retrieve the size in BITS of the given type.
LPPCFUNC u64 getTypeSize(Context* ctx, Type* type);

LPPCFUNC b8 typeIsBuiltin(Type* type);

// Get the declaration of this type if applicable.
// Returns nullptr if this is not possible, eg. 
// if this is a builtin type.
LPPCFUNC Decl* getTypeDecl(Type* type);

/* ----------------------------------------------------------------------------
 *  Dumps the given type.
 */
LPPCFUNC void dumpType(Type* type);

LPPCFUNC FieldIter* createFieldIter(Context* ctx, Decl* decl);
LPPCFUNC Decl*      getNextField(FieldIter* iter);


LPPCFUNC EnumIter* createEnumIter(Context* ctx, Decl* decl);
LPPCFUNC Decl*     getNextEnum(EnumIter* iter);

/* ----------------------------------------------------------------------------
 */
LPPCFUNC s64 getEnumValue(Decl* decl);

/* ----------------------------------------------------------------------------
 |  Asks clang to dump the loaded ast to stdout for debug purposes.
 */
LPPCFUNC void dumpAST(Context* ctx);

/* ----------------------------------------------------------------------------
 |  Retrieves clang's internal naming of the given decl for debugging decl 
 |  kinds unknown to the api.
 */
LPPCFUNC String getClangDeclSpelling(Decl* decl);

//!ffiapi end

LPPCFUNC void playground();

}

#endif
