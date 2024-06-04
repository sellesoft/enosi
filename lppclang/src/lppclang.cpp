#include "lppclang.h"

#include "iro/io/io.h"
#include "iro/fs/file.h"
#include "iro/logger.h"
#include "iro/memory/bump.h"
#include "iro/containers/pool.h"
#include "iro/containers/list.h"
#include "iro/containers/linked_pool.h"

#include "clang/Tooling/Tooling.h"
#include "clang/AST/Decl.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Parse/ParseAST.h"
#include "clang/Lex/Lexer.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/Support/raw_ostream.h"

#include "assert.h"

#undef stdout

template<typename T>
using Rc = llvm::IntrusiveRefCntPtr<T>;

template<typename T>
using Up = std::unique_ptr<T>;

template<typename T>
using Sp = std::shared_ptr<T>;

Logger logger = Logger::create("lppclang"_str, Logger::Verbosity::Trace);

// implement a llvm raw ostream to capture output from clang/llvm stuff
struct LLVMIO : llvm::raw_ostream
{
	io::IO* io;

	LLVMIO(io::IO* io) : io(io) {}

	void write_impl(const char* ptr, size_t size) override
	{
		io->write({(u8*)ptr, size});
	}

	uint64_t current_pos() const override 
	{
		return 0;
	}
};

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

struct Lexer;

typedef DLinkedPool<Lexer> LexerPool;

struct Lexer
{
	LexerPool::Node* node;
	clang::FileID fileid;
	clang::Lexer* lexer;
	clang::Token token;
	b8 at_end;
};

struct Context
{
	ASTUnitPtr ast_unit;

	LLVMIO* io;

	// diagnositics handling
	// TODO(sushi) could make a way later to communicate diags to 
	//             stuff using lppclang, but it probably wouldn't be very useful
	clang::DiagnosticOptions diagopts;
	clang::TextDiagnosticPrinter* diagprinter;
	Rc<clang::DiagnosticsEngine> diagengine;

	// fake filesystem to load strings into clang/llvm stuff
	Rc<llvm::vfs::OverlayFileSystem> overlayfs;
	Rc<llvm::vfs::InMemoryFileSystem> inmemfs;

	Rc<clang::FileManager> filemgr;

	clang::SourceManager* srcmgr;

	LexerPool lexers;

	// Allocate things into a bump allocator to avoid burdening
	// the user with the responsibility of cleaning stuff up.
	// Later on the api might be redesigned to allow the user 
	// to perform cleanup but I don't want to deal with that right now.
	// TODO(sushi) because this api is intended to be used via lua, this 
	//             should probably be changed to a conventional allocator
	//             and resources can be properly tracked via luajit's finalizers.
	mem::Bump allocator;

	b8 init()
	{
		if (!allocator.init())
			return false;

		lexers.init(&allocator);

		io = new LLVMIO(&fs::stdout);
		diagprinter = new clang::TextDiagnosticPrinter(*io, &diagopts);
		diagengine = clang::CompilerInstance::createDiagnostics(&diagopts, diagprinter, false);

		// not sure if putting this stuff into the bump allocator is ok
		overlayfs = new llvm::vfs::OverlayFileSystem(llvm::vfs::getRealFileSystem());
		inmemfs = new llvm::vfs::InMemoryFileSystem;
		overlayfs->pushOverlay(inmemfs);

		filemgr = new clang::FileManager({}, overlayfs);

		srcmgr = new clang::SourceManager(*diagengine, *filemgr);
		diagengine->setSourceManager(srcmgr);

		return true;
	}

	void deinit()
	{
		delete io;
		delete srcmgr;
		delete diagprinter;
		for (auto& l : lexers)
			delete l.lexer;
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

/* ------------------------------------------------------------------------------------------------ createLexer
 */
Lexer* createLexer(Context* ctx, str s)
{
	assert(ctx);

	auto lexer_node = ctx->lexers.pushHead();
	auto lexer = lexer_node->data;
	lexer->node = lexer_node;

	lexer->fileid = // TODO(sushi) this could prob be cleaned up when the lexer is destroyed
		ctx->srcmgr->createFileID(
			llvm::MemoryBuffer::getMemBuffer(llvm::StringRef((char*)s.bytes, s.len)));

	lexer->lexer =
		new clang::Lexer(
			lexer->fileid, 
			ctx->srcmgr->getBufferOrFake(lexer->fileid), 
			*ctx->srcmgr, 
			lang_options);

	lexer->lexer->SetCommentRetentionState(true);
	lexer->lexer->SetKeepWhitespaceMode(true);

	return lexer; // TODO(sushi) could just pass the node but causes weird typing
}

/* ------------------------------------------------------------------------------------------------ destroyLexer
 */
void destroyLexer(Context* ctx, Lexer* lexer)
{
	assert(ctx and lexer);

	delete lexer->lexer;
	ctx->lexers.remove(lexer->node);
}

/* ------------------------------------------------------------------------------------------------ lexerNextToken
 */
Token* lexerNextToken(Lexer* lexer)
{
	assert(lexer);

	if (lexer->at_end)
		return nullptr;
	
	if (lexer->lexer->LexFromRawLexer(lexer->token))
		lexer->at_end = true;

	return (Token*)&lexer->token;
}

/* ------------------------------------------------------------------------------------------------ tokenGetRaw
 */
str tokenGetRaw(Context* ctx, Lexer* l, Token* t)
{
	assert(t);

	auto tok = (clang::Token*)t;

	auto beginoffset = ctx->srcmgr->getDecomposedLoc(tok->getLocation()).second;
	auto endoffset = ctx->srcmgr->getDecomposedLoc(tok->getEndLoc()).second;
	auto membuf = ctx->srcmgr->getBufferOrFake(l->fileid);
	auto buf = membuf.getBuffer().substr(beginoffset, endoffset-beginoffset);
	return {(u8*)buf.data(), buf.size()};
}

/* ------------------------------------------------------------------------------------------------ tokenGetKind
 */
TokenKind tokenGetKind(Token* t)
{
	assert(t);

	using CTK = clang::tok::TokenKind;

	auto kind = ((clang::Token*)t)->getKind();

	switch (kind)
	{
#define map(x, y) case CTK::x: return GLUE(TK_,y)

	map(eof, EndOfFile);
	map(comment, Comment);
	map(identifier, Identifier);
	map(raw_identifier, Identifier);
	map(numeric_constant, NumericConstant);
	map(char_constant, CharConstant);
	map(string_literal, StringLiteral);
	map(unknown, Whitespace); // just assume this is whitespace, kinda stupid its marked this way
							  // if this causes problems then maybe confirm it's whitespace by 
							  // searching the raw later
	// TODO(sushi) MAYBE implement the rest of these, but not really neccessary cause for punc
	//             and keyword we can just string compare in lua.

#undef map
	}
	return TK_Unhandled;
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
			std::make_shared<clang::PCHContainerOperations>(),
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

/* ------------------------------------------------------------------------------------------------ getDeclBegin
 */
u64 getDeclBegin(Context* ctx, Decl* decl)
{
	auto [_, offset] = ctx->getASTContext()->getSourceManager().getDecomposedSpellingLoc(getClangDecl(decl)->getBeginLoc());
	return offset;
}

/* ------------------------------------------------------------------------------------------------ getDeclEnd
 */
u64 getDeclEnd(Context* ctx, Decl* decl)
{
	auto [_, offset] = ctx->getASTContext()->getSourceManager().getDecomposedSpellingLoc(getClangDecl(decl)->getEndLoc());
	return offset;
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

/* ------------------------------------------------------------------------------------------------ tagIsEmbeddedInDeclarator
 */
b8 tagIsEmbeddedInDeclarator(Decl* decl)
{
	auto cdecl = getClangDecl(decl);
	if (!clang::TagDecl::classof(cdecl))
		return false;
	return ((clang::TagDecl*)cdecl)->isEmbeddedInDeclarator();
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


Decl* testIncParse(str s)
{
	using namespace clang;
	using namespace llvm;

	// output to stdout for now
	LLVMIO io(&fs::stdout);

	// setup diagnostics engine
	DiagnosticOptions diag_options;
	TextDiagnosticPrinter diag_printer(io, &diag_options);
	Rc<DiagnosticsEngine> diag_engine = 
		CompilerInstance::createDiagnostics(&diag_options, &diag_printer, false);

	// setup a fake file system to trick clang with
	Rc<vfs::OverlayFileSystem> overlayfs = new vfs::OverlayFileSystem(vfs::getRealFileSystem());
	Rc<vfs::InMemoryFileSystem> inmemfs = new vfs::InMemoryFileSystem;
	overlayfs->pushOverlay(inmemfs);
	Rc<FileManager> file_mgr = new FileManager({}, overlayfs);

	SourceManager src_mgr(*diag_engine, *file_mgr);
	diag_engine->setSourceManager(&src_mgr);

	// create a new compiler invocation and instance
	Sp<CompilerInvocation> invocation(new CompilerInvocation);
	Up<CompilerInstance> compiler(new CompilerInstance);
	compiler->setInvocation(invocation);
	compiler->setFileManager(&*file_mgr);
	compiler->setDiagnostics(&*diag_engine);
	compiler->setSourceManager(&src_mgr);
	compiler->createPreprocessor(TU_Prefix);
	compiler->createSema(TU_Prefix, nullptr);
	ParseAST(compiler->getSema(), false, false);
	return nullptr;
}
