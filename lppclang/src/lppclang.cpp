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
#include "clang/AST/DeclGroup.h"
#include "clang/Sema/Sema.h"
#include "clang/CodeGen/ObjectFilePCHContainerOperations.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/TextDiagnosticBuffer.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/FrontendTool/Utils.h"
#include "clang/Parse/Parser.h"
#include "clang/Lex/Lexer.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/Compilation.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/Host.h"

#include "assert.h"

#undef stdout

template<typename T>
using Rc = llvm::IntrusiveRefCntPtr<T>;

template<typename T>
using Up = std::unique_ptr<T>;

template<typename T, typename... Args>
inline Up<T> makeUnique(Args&&... args) { return std::make_unique<T>(args...); }

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

inline llvm::StringRef strRef(str s)
{
	return llvm::StringRef((char*)s.bytes, s.len);
}

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
	Rc<clang::DiagnosticOptions> diagopts;
	clang::TextDiagnosticPrinter* diagprinter;

	// fake filesystem to load strings into clang/llvm stuff
	Rc<llvm::vfs::OverlayFileSystem> overlayfs;
	Rc<llvm::vfs::InMemoryFileSystem> inmemfs;

	LexerPool lexers;

	DLinkedPool<str> include_dirs;

	Up<clang::CompilerInstance> clang;

	Up<clang::Parser> parser;

	// Allocate things into a bump allocator to avoid burdening
	// the user with the responsibility of cleaning stuff up.
	// Later on the api might be redesigned to allow the user 
	// to perform cleanup but I don't want to deal with that right now.
	// TODO(sushi) because this api is intended to be used via lua, this 
	//             should probably be changed to a conventional allocator
	//             and resources can be properly tracked via luajit's finalizers.
	mem::Bump allocator;

	// NOTE(sushi) This is pretty much copied directly from the clang-repl thing
	//             and i originally added it cause i thought it would solve a 
	//             problem i was having, but it wound up not being the fix. This
	//             might be desirable to have has it helps properly manage 
	//             different ways of using clang (such as outputting to LLVM, .obj, 
	//             etc. or just parsing syntax to make an AST and such) so I'm gonna
	//             keep it for now, but it is really not necessary.
	struct IncrementalAction : public clang::WrapperFrontendAction
	{
		b8 terminating = false;

		IncrementalAction(clang::CompilerInstance& clang) : 
			clang::WrapperFrontendAction(
				makeUnique<clang::SyntaxOnlyAction>()) {}

		clang::TranslationUnitKind getTranslationUnitKind() override { return clang::TU_Incremental; }

		void ExecuteAction() override
		{
			using namespace clang; 

			CompilerInstance& clang = getCompilerInstance();
			
			clang.getPreprocessor().EnterMainSourceFile();
			if (!clang.hasSema())
				clang.createSema(getTranslationUnitKind(), nullptr);
		}

		void EndSourceFile() override
		{
			if (terminating && WrappedAction.get())
				clang::WrapperFrontendAction::EndSourceFile();
		}

		void FinalizeAction()
		{
			assert(!terminating && "already finalized");
			terminating = true;
			EndSourceFile();
		}
	};

	Up<IncrementalAction> action;

	b8 init()
	{
		using namespace clang;

		if (!allocator.init())
			return false;

		lexers.init(&allocator);

		// not sure if putting this stuff into the bump allocator is ok
		overlayfs = new llvm::vfs::OverlayFileSystem(llvm::vfs::getRealFileSystem());
		inmemfs = new llvm::vfs::InMemoryFileSystem;
		overlayfs->pushOverlay(inmemfs);

		include_dirs.init(&allocator);

		std::vector<const char*> driver_args;

		std::string main_exe_name = 
			llvm::sys::fs::getMainExecutable(nullptr, nullptr);

		driver_args.push_back(main_exe_name.c_str());
		driver_args.push_back("-xc++");
		driver_args.push_back("-Xclang");
		driver_args.push_back("-fincremental-extensions");
		driver_args.push_back("-c");
		driver_args.push_back("lppclang-inputs");

		// Temp diags buffer to properly catch anything that might go wrong in the 
		// driver. Probaby not very useful atm, but if user args are ever supported
		// this may become more important.
		Rc<DiagnosticIDs> diagids(new DiagnosticIDs);
		diagopts = CreateAndPopulateDiagOpts(driver_args);
		TextDiagnosticBuffer* diagbuffer = new TextDiagnosticBuffer;
		DiagnosticsEngine diags(diagids, &*diagopts, diagbuffer);

		driver::Driver driver(driver_args[0], llvm::sys::getProcessTriple(), diags);
		driver.setCheckInputsExist(false);
		Up<clang::driver::Compilation> compilation(
				driver.BuildCompilation(llvm::ArrayRef(driver_args)));

		auto& jobs = compilation->getJobs();
		auto cmd = llvm::cast<driver::Command>(&*jobs.begin());
		auto cc1args = &cmd->getArguments();

		clang = makeUnique<CompilerInstance>();

		io = new LLVMIO(&fs::stdout);
		diagprinter = new TextDiagnosticPrinter(*io, diagopts.get());

		auto& clang = *this->clang.get();
		clang.createDiagnostics(diagprinter, false);

		clang.getPCHContainerOperations()->registerWriter(
				makeUnique<clang::ObjectFilePCHContainerWriter>());
		clang.getPCHContainerOperations()->registerReader(
				makeUnique<clang::ObjectFilePCHContainerReader>());
		
		if (!CompilerInvocation::CreateFromArgs(
				clang.getInvocation(),
				llvm::ArrayRef(cc1args->begin(), cc1args->size()),
				clang.getDiagnostics()))
		{
			ERROR("failed to create compiler invocation\n");
			return false;
		}

		CompilerInvocation& invocation = clang.getInvocation();

		if (clang.getHeaderSearchOpts().UseBuiltinIncludes &&
			clang.getHeaderSearchOpts().ResourceDir.empty())
		{
			clang.getHeaderSearchOpts().ResourceDir =
				clang::CompilerInvocation::GetResourcesPath(
					driver_args[0], nullptr);
		}

		clang.setTarget(
			TargetInfo::CreateTargetInfo(
				clang.getDiagnostics(),
				invocation.TargetOpts));

		if (!clang.hasTarget())
		{
			ERROR("Failed to set compiler target\n");
			return false;
		}

		clang.getTarget().adjust(clang.getDiagnostics(), clang.getLangOpts());

		// add an initially empty file 
		auto mb = llvm::MemoryBuffer::getMemBuffer("").release();
		clang.getPreprocessorOpts().addRemappedFile("lppclang-inputs", mb);

		clang.getCodeGenOpts().ClearASTBeforeBackend = false;
		clang.getFrontendOpts().DisableFree = false;
		clang.getCodeGenOpts().DisableFree = false;

		clang.LoadRequestedPlugins();

		// Old manual setup stuff that sorta half worked. If we ever decide to 
		// stop using the FrontendAction stuff, continue getting this to work.
		//clang.createFileManager(overlayfs);
		//clang.createSourceManager(clang.getFileManager());
		//clang.getDiagnostics().setSourceManager(&clang.getSourceManager());
		//clang.createPreprocessor(clang::TU_Incremental);
		//clang.setASTConsumer(makeUnique<clang::ASTConsumer>());
		//clang.createASTContext();
		//clang.createSema(clang::TU_Incremental, nullptr);

		action = makeUnique<IncrementalAction>(clang);

		clang.ExecuteAction(*action);

		parser = makeUnique<Parser>(clang.getPreprocessor(), clang.getSema(), false);
		parser->Initialize();

		return true;
	}

	void deinit()
	{
		diagprinter->EndSourceFile();

		parser.reset();

		if (clang->hasPreprocessor())
			clang->getPreprocessor().EndSourceFile();

		clang->setSema(nullptr);
		clang->setASTContext(nullptr);
		clang->setASTConsumer(nullptr);

		clang->clearOutputFiles(false);

		clang->setPreprocessor(nullptr);
		clang->setSourceManager(nullptr);
		clang->setFileManager(nullptr);

		delete io;
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

	using namespace clang;

	CompilerInstance& clang = *ctx->clang;
	SourceManager&    srcmgr = clang.getSourceManager();

	lexer->fileid = 
		srcmgr.createFileID(
			llvm::MemoryBuffer::getMemBuffer(strRef(s)));

	lexer->lexer =
		new clang::Lexer(
			lexer->fileid,
			srcmgr.getBufferOrFake(lexer->fileid),
			srcmgr,
			lang_options);

	lexer->lexer->SetCommentRetentionState(true);
	lexer->lexer->SetKeepWhitespaceMode(true);

	return lexer;
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
	assert(ctx and l and t);

	clang::SourceManager& srcmgr = ctx->clang->getSourceManager();
	auto tok = (clang::Token*)t;

	auto beginoffset = srcmgr.getDecomposedLoc(tok->getLocation()).second;
	auto endoffset = srcmgr.getDecomposedLoc(tok->getEndLoc()).second;
	auto membuf = srcmgr.getBufferOrFake(l->fileid);
	auto buf = membuf.getBuffer().substr(beginoffset, endoffset-beginoffset);
	return {(u8*)buf.data(), buf.size()};
}

/* ------------------------------------------------------------------------------------------------ tokenGetRawAndLoc
 */
TokenRawAndLoc tokenGetRawAndLoc(Context* ctx, Lexer* l, Token* t)
{
	assert(ctx and l and t);

	clang::SourceManager& srcmgr = ctx->clang->getSourceManager();
	auto tok = (clang::Token*)t;

	auto beginoffset = srcmgr.getDecomposedLoc(tok->getLocation()).second;
	auto endoffset = srcmgr.getDecomposedLoc(tok->getEndLoc()).second;
	auto membuf = srcmgr.getBufferOrFake(l->fileid);
	auto buf = membuf.getBuffer().substr(beginoffset, endoffset-beginoffset);
	return {{(u8*)buf.data(), buf.size()}, beginoffset, endoffset};
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
							  // TODO(sushi) if i actually do wind up forking clang so i can have local patches
							  //             maybe make this an explicit token
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

	std::vector<std::string> args;

	for (auto& d : ctx->include_dirs)
	{
		std::string arg = "-I";
		arg += std::string((char*)d.bytes, d.len);
		args.push_back(arg);
	}

	ASTUnitPtr unit = 
		clang::tooling::buildASTFromCodeWithArgs(
			llvm::StringRef((char*)s.bytes, s.len), 
			args,
			"lppclang-string.cc",
			"lppclang",
			std::make_shared<clang::PCHContainerOperations>(),
			clang::tooling::getClangStripDependencyFileAdjuster(),
			clang::tooling::FileContentMappings(),
			nullptr);

	if (!unit)
		return false;

	ctx->takeAST(unit);

	return true;
}

/* ------------------------------------------------------------------------------------------------ parseString
 */
Decl* parseString(Context* ctx, str s)
{
	assert(ctx);

	using namespace clang;

	CompilerInstance& clang = *ctx->clang;
	Parser&           parser = *ctx->parser;
	SourceManager&    srcmgr = clang.getSourceManager();
	Preprocessor&     preprocessor = clang.getPreprocessor();
	Sema&             sema = clang.getSema();
	ASTContext&       astctx = sema.getASTContext();

	SourceLocation new_loc = srcmgr.getLocForStartOfFile(srcmgr.getMainFileID());

	FileID fileid = 
		srcmgr.createFileID(
			std::move(llvm::MemoryBuffer::getMemBuffer(llvm::StringRef((char*)s.bytes, s.len))),
			SrcMgr::C_User, 
			0, 0, new_loc);
			
	if (preprocessor.EnterSourceFile(fileid, nullptr, new_loc))
		return nullptr;

	astctx.addTranslationUnitDecl();

	if (parser.getCurToken().is(tok::annot_repl_input_end))
	{
		// reset parser from last incremental parse
		parser.ConsumeAnyToken();
		parser.ExitScope();
		sema.CurContext = nullptr;
		parser.EnterScope(Scope::DeclScope);
		sema.ActOnTranslationUnitScope(parser.getCurScope());
	}

	Parser::DeclGroupPtrTy decl_group;
	Sema::ModuleImportState import_state;

	for (b8 at_eof = parser.ParseFirstTopLevelDecl(decl_group, import_state);
		 !at_eof;
		 at_eof = parser.ParseTopLevelDecl(decl_group, import_state))
	{/* dont do anything idk yet */}

	return (::Decl*)astctx.getTranslationUnitDecl();
}

/* ------------------------------------------------------------------------------------------------ dumpDecl
 */
void dumpDecl(Decl* decl)
{
	assert(decl);
	getClangDecl(decl)->dump();
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

/* ------------------------------------------------------------------------------------------------ 
 */
Type* lookupType(Context* ctx, str name)
{
	using namespace clang;

	CompilerInstance& clang = *ctx->clang;
	Parser&           parser = *ctx->parser;
	Sema&             sema = clang.getSema();
	Preprocessor&     preprocessor = clang.getPreprocessor();

	IdentifierInfo* idinfo = clang.getPreprocessor().getIdentifierInfo(strRef(name));

	DeclarationName dname(idinfo);
	DeclContextLookupResult result = sema.CurContext->lookup(dname);

	for (auto r : result)
	{
		r->dump();	
	}

	return nullptr;
}

// I dont really care for doing this but I dont feel like figuring out their
// error thing right now so whatever.
llvm::ExitOnError exit_on_error;
void addIncludeDir(Context* ctx, str s)
{
	assert(ctx);
	using namespace clang;
	CompilerInstance& clang = *ctx->clang;
	Preprocessor&     preprocessor = clang.getPreprocessor();
	HeaderSearch&     header_search = preprocessor.getHeaderSearchInfo();
	FileManager&      file_manager = clang.getFileManager();

	auto dirent = exit_on_error(file_manager.getDirectoryRef(strRef(s)));

	DirectoryLookup dirlu(dirent, SrcMgr::C_User, false);
	header_search.AddSearchPath(dirlu, false);
}
