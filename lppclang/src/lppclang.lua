--
--
-- lpp extension providing access to clang.
--
--

-- load the generated cdefs file and give them to ffi

local ffi = require "ffi"

ffi.cdef [[

typedef enum
{
  DeclKind_Invalid,
  DeclKind_Unknown,
  DeclKind_Function,
  DeclKind_Method,
  DeclKind_Parameter,
  DeclKind_Variable,
  DeclKind_Field,
  DeclKind_Typedef,
  DeclKind_Record,
  DeclKind_Enum,
} DeclKind;
typedef enum
{
  TK_EndOfFile,
  TK_Unhandled,
  TK_Comment,
  TK_Whitespace,
  TK_Identifier,
  TK_NumericConstant,
  TK_CharConstant,
  TK_StringLiteral,
} TokenKind;
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
typedef struct ParseStmtResult
{
  Stmt* stmt;
  s64 offset;
} ParseStmtResult;
typedef struct ParseDeclResult
{
  Decl* decl;
  s64 offset;
} ParseDeclResult;
typedef struct ParseTopLevelDeclsResult
{
  DeclIter* iter;
  s64 offset;
} ParseTopLevelDeclsResult;
typedef struct ParseTypeNameResult
{
  Type* type;
  s64 offset;
} ParseTypeNameResult;
typedef struct ParseExprResult
{
  Expr* expr;
  s64 offset;
} ParseExprResult;
typedef struct ParseIdentifierResult
{
  String id;
  s64 offset;
} ParseIdentifierResult;
typedef struct
{
  String raw;
  u64 start_offset;
  u64 end_offset;
} TokenRawAndLoc;
 void addIncludeDir(Context* ctx, String s);
 Type* lookupType(Context* ctx, String name);
 ParseStmtResult parseStatement(Context* ctx);
 Decl* getStmtDecl(Context* ctx, Stmt* stmt);
 ParseTypeNameResult parseTypeName(Context* ctx);
 ParseDeclResult parseTopLevelDecl(Context* ctx);
 ParseTopLevelDeclsResult parseTopLevelDecls(Context* ctx);
 ParseExprResult parseExpr(Context* ctx);
 ParseIdentifierResult parseIdentifier(Context* ctx);
 Decl* lookupName(Context* ctx, String s);
 b8 loadString(Context* ctx, String s);
 String getDependencies(String file, String* args, u64 argc);
 void destroyDependencies(String deps);
 b8 beginNamespace(Context* ctx, String name);
 void endNamespace(Context* ctx);
 Context* createContext(String* args, u64 argc);
 void destroyContext(Context*);
 Lexer* createLexer(Context* ctx, String s);
 Token* lexerNextToken(Lexer* l);
 String tokenGetRaw(Context* ctx, Lexer* l, Token* t);
 TokenKind tokenGetKind(Token* t);
 b8 createASTFromString(Context* ctx, String s);
 Decl* parseString(Context* ctx, String s);
 void dumpDecl(Decl* decl);
 Decl* getTranslationUnitDecl(Context* ctx);
 DeclIter* createDeclIter(Context* ctx, Decl* decl);
 Decl* getNextDecl(DeclIter* iter);
 DeclIter* getContextOfDecl(Context* ctx, Decl* decl);
 DeclKind getDeclKind(Decl* decl);
 String getDeclName(Decl* decl);
 Type* getDeclType(Decl* decl);
 Type* getTypeDeclType(Context* ctx, Decl* decl);
 u64 getDeclBegin(Context* ctx, Decl* decl);
 u64 getDeclEnd(Context* ctx, Decl* decl);
 b8 isStruct(Decl* decl);
 b8 isUnion(Decl* decl);
 b8 isEnum(Decl* decl);
 b8 isCanonicalDecl(Decl* decl);
 Type* getFunctionReturnType(Decl* decl);
 b8 functionHasBody(Decl* decl);
 u32 getFunctionBodyBegin(Decl* decl);
 u32 getFunctionBodyEnd(Decl* decl);
 b8 tagHasBody(Decl* decl);
 u32 getTagBodyBegin(Decl* decl);
 u32 getTagBodyEnd(Decl* decl);
 b8 tagIsEmbeddedInDeclarator(Decl* decl);
 ParamIter* createParamIter(Context* ctx, Decl* decl);
 Decl* getNextParam(ParamIter* iter);
 b8 isCanonical(Type* type);
 b8 isUnqualified(Type* type);
 b8 isUnqualifiedAndCanonical(Type* type);
 b8 isConst(Type* type);
 b8 isPointer(Type* type);
 Type* getPointeeType(Type* type);
 b8 isArray(Type* type);
 Type* getArrayElementType(Context* ctx, Type* type);
 u64 getArrayLen(Context* ctx, Type* type);
 Type* getCanonicalType(Type* type);
 Type* getUnqualifiedType(Type* type);
 Type* getUnqualifiedCanonicalType(Type* type);
 String getTypeName(Context* ctx, Type* type);
 String getCanonicalTypeName(Context* ctx, Type* type);
 String getUnqualifiedCanonicalTypeName(Context* ctx, Type* type);
 u64 getTypeSize(Context* ctx, Type* type);
 b8 typeIsBuiltin(Type* type);
 Decl* getTypeDecl(Type* type);
 FieldIter* createFieldIter(Context* ctx, Decl* decl);
 Decl* getNextField(FieldIter* iter);
 u64 getFieldOffset(Context* ctx, Decl* field);
 EnumIter* createEnumIter(Context* ctx, Decl* decl);
 Decl* getNextEnum(EnumIter* iter);
 void dumpAST(Context* ctx);
 String getClangDeclSpelling(Decl* decl);

]]

-- set to the lpp object when loaded
local lpp = require "lpp"

-- contains the shared library when loaded 
local lppclang

-- helpers
local strtype = ffi.typeof("String")
local make_str = function(s)
  return strtype(s, #s)
end

local strToLua = function(s)
  return ffi.string(s.s, s.len)
end

--- General api for using lppclang.
local clang = {}

-- forward declarations
local Ctx,
      Decl,
      Type,
      Function,
      DeclIter,
      ParamIter,
      FieldIter,
      EnumIter,
      Lexer,
      Token,
      Expr,
      Stmt

local makeStruct = function()
  local o = {}
  o.__index = o
  return o
end

--- A clang context which, at the moment, allows incrementally 
--- parsing C++ code.
---
---@class Ctx
---@field lpp lpp
---@field handle userdata Handle to the internal representation of this Ctx
Ctx = makeStruct()

--- Create a new lppclang context.
---
---@param args List Compiler arguments to pass to clang.
---@return Ctx
Ctx.new = function(args)
  local o = setmetatable({}, Ctx)

  local carr = ffi.new("String["..args:len().."]")

  args:eachWithIndex(function(arg, i)
    carr[i-1] = make_str(arg)
  end)
    
  o.handle = assert(lppclang.createContext(carr, args:len()),
    "failed to create lppclang context!")

  -- automatically clean up the context
  ffi.gc(o.handle, function(self)
    lppclang.destroyContext(self)
  end)

  return o
end

--- Parse a given string using the given context.
--- This currently assumes that what is being parsed is in global
--- scope, but supports some extra stuff I'll have to document later.
---
--- A Decl representing the fake translation unit will be returned.
---
-- TODO(sushi) maybe just return the DeclIter over the TU instead
--
---@param code string
---@return Decl
Ctx.parseString = function(self, code)
  return Decl.new(self,
    assert(lppclang.parseString(self.handle, make_str(code)),
      "failed to parse given string!"))
end

--- Loads a string at the end of the context's buffer.
---
---@param code string
Ctx.loadString = function(self, code)
  assert(0 ~= lppclang.loadString(self.handle, make_str(code)))
end

--- Gets the toplevel node of the AST.
---
---@return Decl
Ctx.getTranslationUnitDecl = function(self)
  return Decl.new(self,
    lppclang.getTranslationUnitDecl(self.handle))
end

---@class ParseDeclResult
--- Offset into the current string where parsing stopped or -1 
--- if the end of the buffer was reached.
---@field offset number
--- The decl parsed or nil if an error occured.
---@field decl Decl?

--- Parses a single top level declaration.
---
---@return ParseDeclResult
Ctx.parseTopLevelDecl = function(self)
  local res = lppclang.parseTopLevelDecl(self.handle)
  if 0 == res.decl then
    return {offset=0,decl=nil}
  end
  return {offset=tonumber(res.offset),decl=Decl.new(self, res.decl)}
end

---@class ParseTopLevelDeclsResult
--- Offset into the current string where parsing stopped or -1 
--- if the end of the buffer was reached.
---@field offset number
--- The declarations parsed or nil if an error occured.
---@field iter DeclIter?

--- Parses as many top level decls as possible.
---
---@return ParseDeclResult
Ctx.parseTopLevelDecls = function(self)
  local res = lppclang.parseTopLevelDecls(self.handle)
  if 0 == res.iter then
    print("no iter")
    return {offset=0,iter=nil}
  end
  print("has iter")
  return {offset=tonumber(res.offset),iter=DeclIter.new(self, res.iter)}
end

---@class ParseStmtResult
--- Offset into the current string where parsing stopped or -1 
--- if the end of the buffer was reached.
---@field offset number
--- The statement parsed or nil if an error occured.
---@field stmt Stmt?

--- Parses a single statement.
---
---@return ParseStmtResult
Ctx.parseStmt = function(self)
  local res = lppclang.parseStatement(self.handle)
  if 0 == res.stmt then
    return {offset=0,stmt=nil}
  end
  return {offset=tonumber(res.offset),stmt=Stmt.new(self, res.stmt)}
end

---@class ParseExprResult
--- Offset into the current string where parsing stopped or -1 
--- if the end of the buffer was reached.
---@field offset number
--- The expression parsed or nil if an error occured.
---@field expr Expr?

--- Parses a single expression.
---
---@return ParseExprResult
Ctx.parseExpr = function(self)
  local res = lppclang.parseExpr(self.handle)
  if 0 == res.expr then
    return {offset=0,expr=nil}
  end
  return {offset=tonumber(res.offset),expr=Expr.new(self, res.expr)}
end

---@class ParseTypeNameResult
--- Offset into the current string where parsing stopped or -1 
--- if the end of the buffer was reached.
---@field offset number
--- The type parsed or nil if an error occured.
---@field type Type?

--- Parses a typename.
---
---@return ParseTypeNameResult
Ctx.parseTypeName = function(self)
  local res = lppclang.parseTypeName(self.handle)
  if 0 == res.type then
    return {offset=0,type=nil}
  end
  return {offset=tonumber(res.offset),type=Type.new(self, res.type)}
end

---@class ParseIdentifierResult
--- Offset into the current string where parsing stopped or -1 
--- if the end of the buffer was reached.
---@field offset number
--- The identifier parsed.
---@field id string

--- Parses a single C++ identifier.
---
---@return ParseIdentifierResult?
Ctx.parseIdentifier = function(self)
  local res = lppclang.parseIdentifier(self.handle)
  if 0 == res.id.s then
    return nil
  end
  return {offset =  tonumber(res.offset), id = strToLua(res.id)}
end

--- Lex a given string using this context. Note that this does NOT parse
--- the string. This uses a 'raw' lexer, which means that it literally 
--- just gives tokens, no preprocessing or anything like that is done.
---
---@param code string
---@return Lexer
Ctx.lexString = function(self, code)
  return Lexer.new(self,
    assert(lppclang.createLexer(self.handle, make_str(code)),
      "failed to create a Lexer for code \n"..code))
end

--- Lex a given section. This will automatically consume the section 
--- as tokens are retrieved. 
---
-- TODO(sushi) add an options table to thit
---
---@param section Section
---@return Lexer
Ctx.lexSection = function(self, section)
  local lex = self:lexString(assert(section:getString()))
  lex.section = section
  lex.consume = true
  return lex
end

--- Add a path to the include search list.
--- The path must exist, otherwise the program will crash (for now).
---
---@param path string
Ctx.addIncludeDir = function(self, path)
  lppclang.addIncludeDir(self.handle, make_str(path))
end

--- Looks up a type by name.
---
---@param typename string
Ctx.lookupType = function(self, typename)
  local result = lppclang.lookupType(self.handle, make_str(typename))
  if 0 == result then
    return nil
  end
  return Type.new(self, result)
end

--- Begins a namespace with the given name.
--- 
---@param name string
Ctx.beginNamespace = function(self, name)
  lppclang.beginNamespace(self.handle, make_str(name))
end

--- Ends a namespace.
---
Ctx.endNamespace = function(self)
  lppclang.endNamespace(self.handle)
end

--- A parsed declaration.
---
---@class Decl
---@field ctx Ctx The context this Decl was parsed in.
---@field handle userdata Handle to the internal representation of this Decl.
Decl = makeStruct()

--- Construct a new Decl in ctx.
---
---@param ctx Ctx
---@param handle userdata
Decl.new = function(ctx, handle)
  return setmetatable(
  {
    ctx = ctx,
    handle = handle
  }, Decl)
end

--- Assuming that this declaration acts as a context for other declarations,
--- creates and returns a DeclIter over its child decls.
---
--- Returns nil on failure.
---
---@return DeclIter?
Decl.getDeclIter = function(self)
  local iter = lppclang.createDeclIter(self.ctx.handle, self.handle)
  if iter == nil then
    return
  end

  return DeclIter.new(self.ctx, iter)
end

Decl.getFieldIter = function(self)
  return FieldIter.new(self.ctx, 
    lppclang.createFieldIter(self.ctx.handle, self.handle))
end

--- Get a string of the name of this Decl
---
---@return string
Decl.name = function(self)
  local n = lppclang.getDeclName(self.handle)
  return ffi.string(n.s, n.len)
end

--- Retrieve the Type of this declaration.
---
---@return Type?
Decl.type = function(self)
  local t = lppclang.getDeclType(self.handle)
  return Type.new(self.ctx, t)
end

--- Gets the Type representation of this Type declaration.
---
---@return Type?
Decl.getTypeDeclType = function(self)
  return Type.new(self.ctx, 
    lppclang.getTypeDeclType(self.ctx.handle, self.handle))
end

--- Get the offset into the string this Decl was parsed from that
--- it begins at. 
---
---@return number
Decl.getBegin = function(self)
  return assert(tonumber(lppclang.getDeclBegin(self.ctx.handle, self.handle)))
end

--- Get the offset into the string this Decl was parsed from that
--- it ends at.
---
---@return number
Decl.getEnd = function(self)
  return assert(tonumber(lppclang.getDeclEnd(self.ctx.handle, self.handle)))
end

--- Test if this Decl is an enum.
---
---@return boolean
Decl.isEnum = function(self)
  return 0 ~= lppclang.isEnum(self.handle)
end

--- Test if this Decl is a struct or class.
---
---@return boolean
Decl.isStruct = function(self)
  return 0 ~= lppclang.isStruct(self.handle)
end

--- Test if this Decl is a union.
---
---@return boolean
Decl.isUnion = function(self)
  return 0 ~= lppclang.isUnion(self.handle)
end

--- Test if this Decl is a variable.
---
---@return boolean
Decl.isVariable = function(self)
  return lppclang.getDeclKind(self.handle) == lppclang.DeclKind_Variable
end

--- Test if this Decl is a field, a variable that is a part of a struct.
---
---@return boolean
Decl.isField = function(self)
  return lppclang.getDeclKind(self.handle) == lppclang.DeclKind_Field
end

--- Test if this Decl is the canonical declaration of whatever it is 
--- referring to.
---
---@return boolean
Decl.isCanonical = function(self)
  return 0 ~= lppclang.isCanonicalDecl(self.handle)
end

--- Given that this Decl is an enum, creates and returns an iterator
--- over the enum's elements.
---
---@return EnumIter
Decl.getEnumIter = function(self)
  assert(self:isEnum(), "Decl.getEnumIter called on a decl that is not an enum")

  return EnumIter.new(
    self.ctx, 
    assert(lppclang.createEnumIter(self.ctx.handle, self.handle), 
      "failed to create an enum iter!"))
end

--- Dumps clang's pretty printing of this Decl to stdout.
Decl.dump = function(self)
  lppclang.dumpDecl(self.handle)
end

--- If this is a Tag declaration, eg. one that is named (struct, enum, etc.)
--- check if it is embdedded in declarators.
---
--- Eg. this returns true for
--- ```cpp
--- struct Apple {} apple;
--- ```
--- and false for 
--- ```cpp
--- struct Apple {};
--- ```
---
---@return boolean
Decl.tagIsEmbeddedInDeclarator = function(self)
  return 0 ~= lppclang.tagIsEmbeddedInDeclarator(self.handle)
end

Decl.asFunction = function(self)
  if lppclang.getDeclKind(self.decl) == lppclang.DeclKind_Function then
    return Function.new(self.ast, self.decl)
  end
end

--- An iterator over a sequence of declarations.
---
---@class DeclIter
---@field ctx Ctx
---@field handle userdata Handle to the internal representation of this DeclIter
DeclIter = makeStruct()

--- Construct a new DeclIter.
---
---@param ctx Ctx 
---@param handle userdata
DeclIter.new = function(ctx, handle)
  assert(handle ~= nil, "\n  attempt to create a DeclIter from a nil handle")
  return setmetatable(
  {
    ctx = ctx,
    handle = handle,
  }, DeclIter)
end

--- Retrieve the next Decl in the sequence, or nil if there's
--- no more.
---
---@return Decl?
DeclIter.next = function(self)
  if self.handle == nil then
    error(debug.traceback()..
      "\n  DeclIter.next() called on an iter with a nil handle")
  end
  local nd = lppclang.getNextDecl(self.handle)
  if nd ~= nil then
    return Decl.new(self.ctx, nd)
  end
end

--- A C/C++ type.
---
---@class Type
---@field ctx Ctx
---@field handle userdata
Type = makeStruct()

--- Construct a new Type
---
---@param ctx Ctx
---@param handle userdata
Type.new = function(ctx, handle)
  if handle == nil then
    return nil
  end
  return setmetatable(
  {
    ctx = ctx,
    handle = handle
  }, Type)
end

--- Check if this type is canonical.
---
---@return boolean
Type.isCanonical = function(self)
  return 0 ~= lppclang.isCanonical(self.handle)
end

--- Check if this type is unqualified.
---
---@return boolean
Type.isUnqualified = function(self)
  return 0 ~= lppclang.isUnqualified(self.handle)
end

--- Check if this type is unqualified and canonical.
---
---@return boolean
Type.isUnqualifiedAndCanonical = function(self)
  return 0 ~= lppclang.isUnqualifiedAndCanonical(self.handle)
end

--- Check if this type is const.
---
---@return boolean
Type.isConst = function(self)
  return 0 ~= lppclang.isConst(self.handle)
end

--- Retrieve the canonical representation of this Type.
---
---@return Type
Type.getCanonicalType = function(self)
  return Type.new(self.ctx, lppclang.getCanonicalType(self.handle))
end

--- Retrieve the unqualified representation of this Type.
---
---@return Type
Type.getUnqualifiedType = function(self)
  return Type.new(self.ctx, lppclang.getUnqualifiedType(self.handle))
end

--- Retrieve the unqualified canonical representation of this Type.
---
---@return Type
Type.getUnqualifiedCanonicalType = function(self)
  return Type.new(self.ctx, lppclang.getUnqualifiedCanonicalType(self.handle))
end

--- Retrieve the size in bytes of this type.
---
---@return number
Type.size = function(self)
  return assert(tonumber(lppclang.getTypeSize(self.ctx.handle, self.handle)))
end

--- Check if this type is a builting C/C++ type.
---
---@return boolean
Type.isBuiltin = function(self)
  return 0 ~= lppclang.typeIsBuiltin(self.handle)
end

--- Get the offset of a field decl in bits.
---
---@param decl Decl
---@return number
Type.getFieldOffset = function(self, decl)
  assert(decl and decl.handle)
  return tonumber(lppclang.getFieldOffset(self.ctx.handle, decl.handle))
end

Type.getDecl = function(self)
  local result = lppclang.getTypeDecl(self.handle)
  if nil == result then
    return nil
  end
  return Decl.new(self.ctx, result)
end

Type.getTypeName = function(self)
  return strToLua(lppclang.getTypeName(self.ctx.handle, self.handle))
end

Type.getCanonicalTypeName = function(self)
  return strToLua(lppclang.getCanonicalTypeName(self.ctx.handle, self.handle))
end

Type.getUnqualifiedCanonicalTypeName = function(self)
  return strToLua(lppclang.getUnqualifiedCanonicalTypeName(self.ctx.handle, self.handle))
end

Type.isPointer = function(self)
  return 0 ~= lppclang.isPointer(self.handle)
end

Type.getPointeeType = function(self)
  return Type.new(self.ctx, lppclang.getPointeeType(self.handle))
end

Type.isEnum = function(self)
  return 0 ~= lppclang.isEnum(self.handle)
end

Type.isArray = function(self)
  return 0 ~= lppclang.isArray(self.handle)
end

Type.getArrayElementType = function(self)
  return Type.new(self.ctx, 
    lppclang.getArrayElementType(self.ctx.handle, self.handle))
end

Type.getArrayLen = function(self)
  return tonumber(lppclang.getArrayLen(self.ctx.handle, self.handle))
end

EnumIter = {}
EnumIter.__index = EnumIter

EnumIter.new = function(ast, iter)
  assert(iter, "EnumIter.new called without an enum iter pointer")

  local o = {}
  setmetatable(o, EnumIter)
  o.ast = ast
  o.iter = iter
  return o
end

EnumIter.next = function(self)
  local ne = lppclang.getNextEnum(self.iter)
  if ne == nil then
    return nil
  end
  return Decl.new(self.ast, ne)
end

FieldIter = {}
FieldIter.__index = FieldIter

FieldIter.new = function(ctx, handle)
  if handle == nil then
    return
  end
  local o = {}
  o.ctx = ctx
  o.handle = handle
  return setmetatable(o, FieldIter)
end

FieldIter.next = function(self)
  local n = lppclang.getNextField(self.handle)
  if n == nil then
    return
  end
  return Decl.new(self.ctx, n)
end

--- An instance of a clang Lexer over some string.
---
---@class Lexer
---@field ctx Ctx
---@field handle userdata 
---@field curt userdata
---@field section Section
---@field consume boolean
---@field keep_whitespace boolean
Lexer = {}
Lexer.__index = Lexer

Lexer.new = function(ctx, handle)
  assert(ctx and handle)

  return setmetatable(
  {
    ctx=ctx,
    handle=handle,
    curt=nil,
  }, Lexer)
end

Lexer.next = function(self)
  self.curt = lppclang.lexerNextToken(self.handle)
  if self.curt ~= nil then
    if self.section and self.consume then
      self.section:consumeFromBeginning(#self:curtRaw())
    end
    if not self.keep_whitespace and self:at(Lexer.whitespace) then
      return self:next()
    end
    return true
  else
    self.curt = nil
  end
end

Lexer.curtRaw = function(self)
  return strToLua(lppclang.tokenGetRaw(self.ctx.handle, self.handle, self.curt))
end

Lexer.eof = 0
Lexer.identifier = 1
Lexer.comment = 2
Lexer.number = 3
Lexer.char = 4
Lexer.string = 5
Lexer.whitespace = 6
Lexer.other = 7

Lexer.curtKind = function(self)
  local k = lppclang.tokenGetKind(self.curt)
  if k == lppclang.TK_EndOfFile then return Lexer.eof end
  if k == lppclang.TK_Identifier then return Lexer.identifier end
  if k == lppclang.TK_Comment then return Lexer.comment end
  if k == lppclang.TK_NumericConstant then return Lexer.number end
  if k == lppclang.TK_CharConstant then return Lexer.char end
  if k == lppclang.TK_StringLiteral then return Lexer.string end
  if k == lppclang.TK_Whitespace then return Lexer.whitespace end
  return Lexer.other
end

Lexer.at = function(self, x)
  if not self.curt then return false end
  local xtype = type(x)
  if xtype == "number" then
    -- assume its a thing defined above
    return self:curtKind() == x
  elseif xtype == "string" then
    return self:curtRaw() == x
  else
    error("unhandled type passed to Lexer.at: "..xtype, 2)
  end
end

--- A parsed C++ statement.
---@class Stmt
--- The context this statement was parsed in.
---@field ctx Ctx
--- A handle to the internal representation of this Stmt
---@field handle userdata
Stmt = makeStruct()

--- Construct a new Stmt
---@param ctx Ctx
---@param handle userdata
Stmt.new = function(ctx, handle)
  return setmetatable(
  {
    ctx=ctx,
    handle=handle,
  }, Stmt)
end

--- A parsed C expression.
---@class Expr
--- The context this statement was parsed in.
---@field ctx Ctx
--- A handle to the internal representation of this Stmt
---@field handle userdata
Expr = makeStruct()

--- Construct a new Expr
---@param ctx Ctx
---@param handle userdata
Expr.new = function(ctx, handle)
  return setmetatable(
  {
    ctx=ctx,
    handle=handle,
  }, Stmt)
end

-- TODO(sushi) try not to make multiple contexts or leave it to the user to manage them
local createContext = function()
  local ctx = lppclang.createContext()

  if not ctx then
    error("failed to create an lppclang context", 2)
  end

  ffi.gc(ctx, function(self)
    lppclang.destroyContext(self)
  end)

  return ctx
end

clang.lexString = function(str)
  local ctx = createContext()
  if not ctx then return end
  return Lexer.new(ctx, lppclang.createLexer(ctx, make_str(str)))
end

clang.lexSection = function(sec, options)
  options = options or {}

  local ctx = createContext()

  local l = Lexer.new(ctx, lppclang.createLexer(ctx, make_str(sec:getString())))

  l.section = sec
  l.consume = options.consume

  return l
end

clang.parseString = function(str, options)
  options = options or {}

  local ctx = createContext()

  if options.include_dirs then
    for _,d in ipairs(options.include_dirs) do
      lppclang.addIncludeDir(ctx, make_str(d))
    end
  end

  if lppclang.createASTFromString(ctx, make_str(str)) == 0 then
    error("failed to create AST from string", 2)
  end

  return AST.new(ctx)
end

---@param args List
clang.createContext = function(args)
  return Ctx.new(args)
end

clang.generateMakeDepFile = function(file, args)
  local carr = ffi.new("String["..args:len().."]")

  args:eachWithIndex(function(arg, i)
    carr[i-1] = make_str(arg)
  end)

  local result = lppclang.getDependencies(
      make_str(file), 
      carr,
      args:len())

  local out = strToLua(result)

  -- kinda dumb
  lppclang.destroyDependencies(result)

  return out
end

local loaded = false

return
{

  init = function(libpath)
    if loaded then return true end

    lppclang = ffi.load(libpath)

    -- patch lpp with the clang module
    lpp.clang = clang

    loaded = true

    return true
  end

}
