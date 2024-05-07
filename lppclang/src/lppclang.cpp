#include "lppclang.h"
#include "containers/pool.h"
#include "containers/list.h"

#include "memory/bump.h"

#include "clang/Tooling/Tooling.h"
#include "clang/AST/Decl.h"

#include "logger.h"

#include "assert.h"

Logger logger = Logger::create("lppclang"_str, Logger::Verbosity::Trace);

// TODO(sushi) maybe make adjustable later
auto lang_options = clang::LangOptions();
auto printing_policy = clang::PrintingPolicy(lang_options);

// I FUCKING HATE STDC++!!!!
typedef std::unique_ptr<clang::ASTUnit> ASTUnitPtr;

template<typename T>
struct ClangIteratorPool
{
	struct Range
	{
		T current;
		T end;
	};

	Pool<Range> ranges;

	static ClangIteratorPool<T> create()
	{
		ClangIteratorPool<T> out = {};
		out.ranges = Pool<Range>::create();
		return out;
	}

	Range* add()
	{
		return ranges.add();
	}

	void remove(Range* x)
	{
		ranges.remove(x);
	}
};

template<typename T>
struct ClangIter
{
	T current;
	T end;
	
	T::value_type next()
	{
		if (current == end)
			return nullptr;

		auto out = *current;
		current++;
		return out;
	}
};

typedef ClangIter<clang::DeclContext::decl_iterator> DeclIterator;
typedef ClangIter<clang::RecordDecl::field_iterator> FieldIterator;
typedef ClangIter<clang::EnumDecl::enumerator_iterator> EnumIterator;

// dumb special case
struct ParamIterator
{
	clang::FunctionDecl::param_iterator current;
	clang::FunctionDecl::param_iterator end;

	clang::ParmVarDecl* next()
	{
		if (current == end)
			return nullptr;
		
		auto out = *current;
		current++;
		return out;
	}
};

using ClangTypeClass = clang::Type::TypeClass;
using ClangNestedNameSpecifierKind = clang::NestedNameSpecifier::SpecifierKind;

struct Context
{

	ASTUnitPtr ast_unit;

	// Allocate things into a bump allocator to avoid burdening
	// the user with the responsibility of cleaning stuff up.
	// Later on the api might be redesigned to allow the user 
	// to perform cleanup but I don't want to deal with that right now.
	mem::Bump allocator;

	b8 init()
	{
		if (!allocator.init())
			return false;
		return true;
	}

	void deinit()
	{
		allocator.deinit();
	}

	template<typename T>
	T* allocate()
	{
		return (T*)allocator.allocate(sizeof(T));
	}

	void takeAST(ASTUnitPtr& ptr)
	{
		ptr.swap(ast_unit);
	}

	clang::ASTContext* getASTContext()
	{
		return &ast_unit->getASTContext();
	}
};

/* ------------------------------------------------------------------------------------------------ createContext
 */
Context* createContext()
{
	auto ctx = new Context;
	if (!ctx->init())
		return nullptr;
	return ctx;
}

/* ------------------------------------------------------------------------------------------------ destroyContext
 */
void destroyContext(Context* ctx)
{
	ctx->deinit();
	delete ctx;
}

/* ------------------------------------------------------------------------------------------------ getClangDecl
 */
static clang::Decl* getClangDecl(Decl* decl)
{
	return (clang::Decl*)decl;
}

/* ------------------------------------------------------------------------------------------------ getClangType
 */
static clang::QualType getClangType(Type* type)
{
	return clang::QualType::getFromOpaquePtr(type);
}

/* ------------------------------------------------------------------------------------------------ createASTFromString
 */
b8 createASTFromString(Context* ctx, str s)
{	
	assert(ctx);

	struct SuppressDiagnostics : clang::DiagnosticConsumer
	{
		// suppress diags being output to stdout
		void HandleDiagnostic(clang::DiagnosticsEngine::Level DiagLevel, const clang::Diagnostic &Info) {}
	};
	SuppressDiagnostics suppressor;

	ASTUnitPtr unit = 
		clang::tooling::buildASTFromCodeWithArgs(
			llvm::StringRef((char*)s.bytes, s.len), 
			std::vector<std::string>(),
			"lppclang-string.cc",
			"lppclang",
			std::move(std::make_shared<clang::PCHContainerOperations>()),
			clang::tooling::getClangStripDependencyFileAdjuster(),
			clang::tooling::FileContentMappings(),
			&suppressor);

	if (!unit)
		return false;

	ctx->takeAST(unit);

	return true;
}

/* ------------------------------------------------------------------------------------------------ dumpAST
 */
void dumpAST(Context* ctx)
{
	assert(ctx);
	getClangDecl(getTranslationUnitDecl(ctx))->dumpColor();
}

/* ------------------------------------------------------------------------------------------------ getTranslationUnitDecl
 */
Decl* getTranslationUnitDecl(Context* ctx)
{
	return (Decl*)ctx->getASTContext()->getTranslationUnitDecl();
}

/* ------------------------------------------------------------------------------------------------ createASTDeclIter
 */
DeclIter* createDeclIter(Context* ctx, Decl* decl)
{
	assert(decl);

	auto cdecl = getClangDecl(decl);
	if (!clang::DeclContext::classof(cdecl))
		return nullptr;

	auto dctx = clang::Decl::castToDeclContext(cdecl);
	if (dctx->decls_empty())
		return nullptr;

	auto range = ctx->allocate<DeclIterator>();
	range->current = dctx->decls_begin();
	range->end = dctx->decls_end();
	return (DeclIter*)range;
}

/* ------------------------------------------------------------------------------------------------ getNextDecl
 */
Decl* getNextDecl(DeclIter* iter)
{
	assert(iter);
	auto diter = ((DeclIterator*)iter);
	auto out = diter->next();
	if (!out)
		return nullptr;
	if (out->isImplicit()) // only really care about stuff thats actually written here
		return getNextDecl(iter);
	return (Decl*)out;
}

/* ------------------------------------------------------------------------------------------------ getDeclKind
 */
DeclKind getDeclKind(Decl* decl)
{
	assert(decl);

	auto cdecl = (clang::Decl*)decl;

	auto kind = cdecl->getKind();

	switch (kind)
	{
#define kindcase(x,y) case clang::Decl::Kind::x: return DeclKind_##y

		kindcase(Function,  Function);
		kindcase(CXXMethod, Method);
		kindcase(ParmVar,   Parameter);
		kindcase(Var,       Variable);
		kindcase(Field,     Field);
		kindcase(Typedef,   Typedef);
		kindcase(Enum,      Enum);
		kindcase(Record,    Record);
		kindcase(CXXRecord, Record); // not handling C++ specific stuff atm

#undef kindcase
	}

	return DeclKind_Unknown;
}

/* ------------------------------------------------------------------------------------------------ getDeclName
 */
str getDeclName(Decl* decl)
{
	assert(decl);

	auto cdecl = getClangDecl(decl);
	if (!clang::NamedDecl::classof(cdecl))
		return {};

	auto ndecl = (clang::NamedDecl*)cdecl;
	auto name = ndecl->getName();
	return {(u8*)name.data(), name.size()};
}

/* ------------------------------------------------------------------------------------------------ getDeclType
 */
Type* getDeclType(Decl* decl)
{
	auto cdecl = getClangDecl(decl);

	if (!clang::ValueDecl::classof(cdecl))
		return nullptr;

	auto vdecl = (clang::ValueDecl*)cdecl;
	return (Type*)vdecl->getType().getAsOpaquePtr();
}

/* ------------------------------------------------------------------------------------------------ getClangDeclSpelling
 */
str getClangDeclSpelling(Decl* decl)
{
	assert(decl);
	const char* s = getClangDecl(decl)->getDeclKindName();
	return {(u8*)s, strlen(s)};
}

/* ------------------------------------------------------------------------------------------------ getTypeDeclType
 */
Type* getTypeDeclType(Context* ctx, Decl* decl)
{
	assert(ctx && decl);

	auto cdecl = getClangDecl(decl);
	if (!clang::TypeDecl::classof(cdecl))
		return nullptr;

	auto tdecl = (clang::TypeDecl*)cdecl;
	return (Type*)ctx->getASTContext()->getTypeDeclType(tdecl).getAsOpaquePtr();
}

/* ------------------------------------------------------------------------------------------------ getFunctionReturnType
 */
Type* getFunctionReturnType(Decl* decl)
{
	assert(decl);
	
	auto cdecl = getClangDecl(decl);
	if (!clang::FunctionDecl::classof(cdecl))
		return nullptr;

	return (Type*)cdecl->getAsFunction()->getReturnType().getAsOpaquePtr();
}

/* ------------------------------------------------------------------------------------------------ functionHasBody
 */
b8 functionHasBody(Decl* decl)
{
	assert(decl);
	return getClangDecl(decl)->hasBody();
}

/* ------------------------------------------------------------------------------------------------ getFunctionBodyBegin
 */
u32 getFunctionBodyBegin(Decl* decl)
{
	if (!functionHasBody(decl))
		return (u32)-1;
	return getClangDecl(decl)->getBody()->getBeginLoc().getRawEncoding();
}

/* ------------------------------------------------------------------------------------------------ getFunctionBodyEnd
 */
u32 getFunctionBodyEnd(Decl* decl)
{
	if (!functionHasBody(decl))
		return (u32)-1;
	return getClangDecl(decl)->getBody()->getEndLoc().getRawEncoding();
}

/* ------------------------------------------------------------------------------------------------ tagHasBody
 */
b8 tagHasBody(Decl* decl)
{
	assert(decl);

	auto cdecl = getClangDecl(decl);
	if (!clang::TagDecl::classof(cdecl))
		return false;

	auto tdecl = (clang::TagDecl*)cdecl;
	return tdecl->isThisDeclarationADefinition();
}

/* ------------------------------------------------------------------------------------------------ getTagBodyBegin
 */
u32 getTagBodyBegin(Decl* decl)
{
	if (!tagHasBody(decl))
		return (u32)-1;
	auto tdecl = (clang::TagDecl*)getClangDecl(decl);
	return tdecl->getBraceRange().getBegin().getRawEncoding();
}

/* ------------------------------------------------------------------------------------------------ getTagBodyEnd
 */
u32 getTagBodyEnd(Decl* decl)
{
	if (!tagHasBody(decl))
		return (u32)-1;
	auto tdecl = (clang::TagDecl*)getClangDecl(decl);
	return tdecl->getBraceRange().getEnd().getRawEncoding();
}

/* ------------------------------------------------------------------------------------------------ createParamIter
 */
ParamIter* createParamIter(Context* ctx, Decl* decl)
{
	assert(decl);

	auto cdecl = getClangDecl(decl);
	if (!clang::FunctionDecl::classof(cdecl))
		return nullptr;

	auto fdecl = (clang::FunctionDecl*)cdecl;

	if (fdecl->param_empty())
		return nullptr;

	auto iter = ctx->allocate<ParamIterator>();

	iter->current = fdecl->param_begin();
	iter->end = fdecl->param_end();

	return (ParamIter*)iter;
}

/* ------------------------------------------------------------------------------------------------ getNextParam
 */
Decl* getNextParam(ParamIter* iter)
{
	assert(iter);
	return (Decl*)((ParamIterator*)iter)->next();
}

/* ------------------------------------------------------------------------------------------------ isCanonical
 */
b8 isCanonical(Type* type)
{
	return getClangType(type).isCanonical();
}

/* ------------------------------------------------------------------------------------------------ isUnqualified
 */
b8 isUnqualified(Type* type)
{
	return !getClangType(type).hasQualifiers();
}

/* ------------------------------------------------------------------------------------------------ isUnqualifiedAndCanonical
 */
b8 isUnqualifiedAndCanonical(Type* type)
{
	return isUnqualified(type) && isCanonical(type);
}

b8 isUnqualifiedAndCanonical(Type* type);

/* ------------------------------------------------------------------------------------------------ getCanonicalType
 */
Type* getCanonicalType(Type* type)
{
	assert(type);
	return (Type*)getClangType(type).getCanonicalType().getAsOpaquePtr();
}

/* ------------------------------------------------------------------------------------------------ getUnqualifiedType
 */
Type* getUnqualifiedType(Type* type)
{
	assert(type);
	return (Type*)getClangType(type).getUnqualifiedType().getAsOpaquePtr();
}

/* ------------------------------------------------------------------------------------------------ getUnqualifiedCanonicalType
 */
Type* getUnqualifiedCanonicalType(Type* type)
{
	assert(type);
	return getUnqualifiedType(getCanonicalType(type));
}

/* ------------------------------------------------------------------------------------------------ getTypeClass
 */
TypeClass getTypeClass(Type* type)
{
	assert(type);

	auto ctype = getClangType(type);
	auto tc = ctype->getTypeClass();
	switch (tc)
	{
#define classcase(x) case clang::Type::TypeClass::x

	classcase(Elaborated):    return TypeClass_Elaborated;
	classcase(DependentName): return TypeClass_Dependent;
	classcase(Typedef):       return TypeClass_Typedef;	
	classcase(Builtin):       return TypeClass_Builtin;
	classcase(Record):        return TypeClass_Structure;
	classcase(Enum):          return TypeClass_Enum;

#undef classcase
	}

	return TypeClass_Unknown;
}

/* ------------------------------------------------------------------------------------------------ getQualTypeName
 */
str getTypeName(Context* ctx, Type* type)
{
	assert(ctx && type);

	auto ctype = getClangType(type);

	// yeah this kiiiinda sucks.
	// This whole process looks like it takes 3 copies. Maybe there's
	// someway to just pass clang a buffer to write into rather than
	// extracting it from a std::string like if we implement 
	// llvm::raw_ostream or something.
	auto stdstr = ctype.getAsString();
	io::Memory m;
	m.open(stdstr.size(), &ctx->allocator);
	m.write({(u8*)stdstr.data(), stdstr.size()});

	return m.asStr();
}

/* ------------------------------------------------------------------------------------------------ getCanonicalTypeName
 */
str getCanonicalTypeName(Context* ctx, Type* type)
{
	assert(ctx && type);
	return getTypeName(ctx, getCanonicalType(type));
}

/* ------------------------------------------------------------------------------------------------ getUnqualifiedTypeName
 */
str getUnqualifiedTypeName(Context* ctx, Type* type)
{
	assert(ctx && type);
	return getTypeName(ctx, getUnqualifiedType(type));
}

/* ------------------------------------------------------------------------------------------------ getUnqualifiedCanonicalTypeName
 */
str getUnqualifiedCanonicalTypeName(Context* ctx, Type* type)
{
	assert(ctx && type);
	return getTypeName(ctx, getUnqualifiedCanonicalType(type));
}

/* ------------------------------------------------------------------------------------------------ getTypeSize
 */
u64 getTypeSize(Context* ctx, Type* type)
{
	assert(ctx && type);
	return ctx->getASTContext()->getTypeInfo(getClangType(type)).Width;
}

/* ------------------------------------------------------------------------------------------------ typeIsBuiltin
 */
b8 typeIsBuiltin(Type* type)
{
	assert(type);
	return getClangType(type)->getTypeClass() == ClangTypeClass::Builtin;
}

/* ------------------------------------------------------------------------------------------------ getTypeDecl
 */
Decl* getTypeDecl(Type* type)
{
	assert(type);

	auto ctype = getClangType(type);
	if (!clang::TagType::classof(ctype.getTypePtr())) // TODO(sushi) not sure if any other types have decls
		return nullptr;

	return (Decl*)ctype->getAsTagDecl();
}

/* ------------------------------------------------------------------------------------------------ createFieldIter
 */
FieldIter* createFieldIter(Context* ctx, Decl* decl)
{
	assert(decl);

	auto cdecl = getClangDecl(decl);
	auto rdecl = (clang::RecordDecl*)cdecl;

	if (rdecl->field_empty())
		return nullptr;

	auto iter = ctx->allocate<FieldIterator>();
	iter->current = rdecl->field_begin();
	iter->end = rdecl->field_end();
	return (FieldIter*)iter;
}

/* ------------------------------------------------------------------------------------------------ getNextField
 */
Decl* getNextField(FieldIter* iter)
{
	assert(iter);
	return (Decl*)((FieldIterator*)iter)->next();
}

/* ------------------------------------------------------------------------------------------------ getFieldOffset
 */
u64 getFieldOffset(Context* ctx, Decl* field)
{
	assert(ctx && field);

	auto cdecl = getClangDecl(field);
	if (!clang::ValueDecl::classof(cdecl))
		return -1;

	auto vdecl = (clang::ValueDecl*)cdecl;

	return ctx->getASTContext()->getFieldOffset(vdecl);
}

/* ------------------------------------------------------------------------------------------------ createEnumIter
 */
EnumIter* createEnumIter(Context* ctx, Decl* decl)
{
	assert(decl);

	auto cdecl = getClangDecl(decl);
	if (!clang::EnumDecl::classof(cdecl))
		return nullptr;

	auto edecl = (clang::EnumDecl*)cdecl;

	auto iter = ctx->allocate<EnumIterator>();
	iter->current = edecl->enumerator_begin();
	iter->end = edecl->enumerator_end();
	return (EnumIter*)iter;
}

/* ------------------------------------------------------------------------------------------------ getNextEnum
 */
Decl* getNextEnum(EnumIter* iter)
{
	assert(iter);
	return (Decl*)((EnumIterator*)iter)->next();
}
