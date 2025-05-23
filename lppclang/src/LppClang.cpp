#include "LppClang.h"

#include "iro/io/IO.h"
#include "iro/fs/File.h"
#include "iro/Logger.h"
#include "iro/memory/Bump.h"
#include "iro/containers/Pool.h"
#include "iro/containers/List.h"
#include "iro/containers/LinkedPool.h"

// TODO(sushi) PLEASE take some time to separate functionality into 
//             different translation units as this thing currently 
//             takes 20s to compile and 30s to link in debug!!

#include "clang/AST/DeclCXX.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/Tooling/DependencyScanning/DependencyScanningTool.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclGroup.h"
#include "clang/AST/LocInfoType.h"
#include "clang/AST/Comment.h"
#include "clang/Sema/Sema.h"
#include "clang/Sema/Template.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Frontend/TextDiagnosticBuffer.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Serialization/ObjectFilePCHContainerReader.h"
#include "clang/CodeGen/ObjectFilePCHContainerWriter.h"
#include "clang/Frontend/DependencyOutputOptions.h"
#include "clang/FrontendTool/Utils.h"
#include "clang/Parse/Parser.h"
#include "clang/Lex/Lexer.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/Compilation.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/TargetParser/Host.h"

#include "assert.h"

#undef stdout

#ifndef ENOSI_CLANG_EXECUTABLE
// We need to know the path to the locally built clang executable 
// so that we can get the proper resource directory. Otherwise things
// like 'stddef.h' won't exsit.
#error "ENOSI_CLANG_EXECUTABLE not defined"
#endif

template<typename T>
using Rc = llvm::IntrusiveRefCntPtr<T>;

template<typename T>
using Up = std::unique_ptr<T>;

template<typename T, typename... Args>
inline Up<T> makeUnique(Args&&... args) { return std::make_unique<T>(args...); }

template<typename T>
using Sp = std::shared_ptr<T>;

static Logger logger = 
  Logger::create("lppclang"_str, Logger::Verbosity::Trace);

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

/* ----------------------------------------------------------------------------
 */
inline llvm::StringRef strRef(String s)
{
  return llvm::StringRef((char*)s.ptr, s.len);
}

/* ----------------------------------------------------------------------------
 */
template<typename To, typename From>
inline To* tryAs(From* ptr)
{
  if (!llvm::isa<To>(ptr))
    return nullptr;
  return llvm::cast<To>(ptr);
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

typedef ClangIter<clang::DeclContext::decl_iterator> DeclContextIterator;
typedef ClangIter<clang::RecordDecl::field_iterator> FieldIterator;
typedef ClangIter<clang::EnumDecl::enumerator_iterator> EnumIterator;

struct BaseIterator
{
  typedef clang::CXXRecordDecl::base_class_iterator I;

  I current;
  I end;

  Type* next()
  {
    if (current == end)
      return nullptr;

    auto out = *current;
    current++;
    return (Type*)out.getType().getAsOpaquePtr();
  }
};

struct DeclGroupIterator
{
  typedef clang::DeclGroupRef::iterator I;

  I current;
  I end;

  clang::Decl* next()
  {
    if (current == end)
      return nullptr;

    auto out = *current;
    current++;
    return out;
  }
};

struct DeclIterator
{
  enum class Kind
  {
    Context,
    Group,
  };

  Kind kind;

  union
  {
    DeclContextIterator context;
    DeclGroupIterator group;
  };

  clang::Decl* next()
  {
    switch (kind)
    {
    case Kind::Context:
      return context.next();
    case Kind::Group:
      return group.next();
    }
  }

  void initContext(clang::DeclContext* ctx)
  {
    kind = Kind::Context;
    context.current = ctx->decls_begin();
    context.end = ctx->decls_end();
  }

  void initGroup(clang::DeclGroupRef g)
  {
    kind = Kind::Group;
    group.current = g.begin();
    group.end = g.end();
  }
};

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

struct TemplateArgIter
{
  const clang::TemplateArgument* current;
  const clang::TemplateArgument* end;

  const clang::TemplateArgument* next()
  {
    if (current == end)
      return nullptr;

    auto out = current;
    current += 1;
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

/* ----------------------------------------------------------------------------
 */
inline static u64 getTokenStartOffset(
    clang::SourceManager& srcmgr, 
    const clang::Token& token)
{
  return srcmgr.getDecomposedSpellingLoc(token.getLocation()).second;
}

/* ----------------------------------------------------------------------------
 */
inline static u64 getTokenEndOffset(
    clang::SourceManager& srcmgr, 
    const clang::Token& token)
{
  return srcmgr.getDecomposedSpellingLoc(token.getEndLoc()).second;
}

struct Context
{
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

  DLinkedPool<String> include_dirs;

  Up<clang::CompilerInstance> clang;

  Up<clang::Parser> parser;

  DLinkedPool<clang::Parser::ParseScope*> parse_scopes;

  DLinkedPool<clang::Decl*> namespace_decls;

  // Allocate things into a bump allocator to avoid burdening
  // the user with the responsibility of cleaning stuff up.
  // Later on the api might be redesigned to allow the user 
  // to perform cleanup but I don't want to deal with that right now.
  // TODO(sushi) because this api is intended to be used via lua, this 
  //             should probably be changed to a conventional allocator
  //             and resources can be properly tracked via luajit's finalizers.
  mem::Bump allocator;

  // For allocating strings used in 'fake code' we hand to clang so that
  // if it needs to print some kinda error message about them we won't have 
  // just deallocated the data.
  mem::LenientBump string_allocator;

  // NOTE(sushi) This is pretty much copied directly from the clang-repl thing
  //             and i originally added it cause i thought it would solve a 
  //             problem i was having, but it wound up not being the fix. This
  //             might be desirable to have has it helps properly manage 
  //             different ways of using clang (such as outputting to LLVM, 
  //             .obj, etc. or just parsing syntax to make an AST and such) 
  //             so I'm gonna keep it for now, but it is really not necessary.
  struct IncrementalAction : public clang::WrapperFrontendAction
  {
    b8 terminating = false;

    IncrementalAction(clang::CompilerInstance& clang) : 
      clang::WrapperFrontendAction(
        makeUnique<clang::SyntaxOnlyAction>()) {}

    clang::TranslationUnitKind getTranslationUnitKind() override 
      { return clang::TU_Incremental; }

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

  struct DiagConsumer : public clang::DiagnosticConsumer
  {
    clang::Parser& parser;
    clang::TextDiagnosticPrinter* printer;

    DiagConsumer(
          clang::Parser& parser,
          clang::TextDiagnosticPrinter* printer)
      : parser(parser), 
        printer(printer) {}

    /* ------------------------------------------------------------------------
     */
    void HandleDiagnostic(
        clang::DiagnosticsEngine::Level diag_level, 
        const clang::Diagnostic& info)
    {
      // TODO(sushi) it would be nice to intercept these but trying to use 
      //             a TextDiagnosticPrinter to perform the actual printing 
      //             crashes for some reason idk try this again later

      // INFO("~~~~~  in diag ~~~~~~\n");
      // INFO(parser.getCurToken().getName(), "\n");
      // parser.getCurScope()->getEntity()->dumpDeclContext();
      // INFO("~~~~~ out diag ~~~~~~\n");
    }
  };

  Up<DiagConsumer> diag_consumer;

  /* --------------------------------------------------------------------------
   */
  b8 init(Slice<String> args)
  {
    using namespace clang;

    if (!allocator.init())
      return false;

    if (!string_allocator.init())
      return false;

    lexers.init(&allocator);
    parse_scopes.init(&allocator);
    namespace_decls.init(&allocator);

    // not sure if putting this stuff into the bump allocator is ok
    overlayfs = 
      new llvm::vfs::OverlayFileSystem(llvm::vfs::getRealFileSystem());
    inmemfs = new llvm::vfs::InMemoryFileSystem;
    overlayfs->pushOverlay(inmemfs);

    include_dirs.init(&allocator);

    std::vector<const char*> driver_args;

    driver_args.push_back(ENOSI_CLANG_EXECUTABLE);

    for (auto arg : args)
    {
      driver_args.push_back((char*)arg.ptr);
    }

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

    driver::Driver driver(
        driver_args[0], llvm::sys::getProcessTriple(), diags);
    driver.setCheckInputsExist(false);

    Up<clang::driver::Compilation> compilation(
        driver.BuildCompilation(llvm::ArrayRef(driver_args)));

    auto& jobs = compilation->getJobs();
    auto cmd = llvm::cast<driver::Command>(&*jobs.begin());
    auto cc1args = &cmd->getArguments();

    clang = makeUnique<CompilerInstance>();

    auto& clang = *this->clang.get();

    io = new LLVMIO(&fs::stdout);
    diagprinter = new TextDiagnosticPrinter(*io, diagopts.get());
    diag_consumer = makeUnique<DiagConsumer>( *parser, diagprinter);

    clang.createDiagnostics(
        *inmemfs,
        diagprinter, 
        false);
    clang.getDiagnostics().setShowColors(true);

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

    auto& header_search = clang.getHeaderSearchOpts();

    b8 use_builtin = header_search.UseBuiltinIncludes;
    auto resource_dir = header_search.ResourceDir;

    if (header_search.UseBuiltinIncludes &&
        header_search.ResourceDir.empty())
    {
      header_search.ResourceDir =
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
    clang.getLangOpts().CommentOpts.ParseAllComments = true;

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

    parser = makeUnique<Parser>(
        clang.getPreprocessor(), clang.getSema(), false);
    parser->Initialize();

    return true;
  }

  /* --------------------------------------------------------------------------
   */
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

    parse_scopes.deinit();
    namespace_decls.deinit();

    string_allocator.deinit();

    delete io;
    delete diagprinter;
    for (auto& l : lexers)
      delete l.lexer;
    allocator.deinit();
  }

  /* --------------------------------------------------------------------------
   */
  template<typename T>
  T* allocate()
  {
    return (T*)allocator.allocate(sizeof(T));
  }

  /* --------------------------------------------------------------------------
   */
  clang::ASTContext* getASTContext()
  {
    return &clang->getASTContext();
  }
};

/* ----------------------------------------------------------------------------
 */
static b8 startNewBuffer(
    clang::SourceManager& srcmgr, 
    clang::Preprocessor& preprocessor,
    String s)
{
  using namespace clang;

  SourceLocation new_loc = srcmgr.getLocForEndOfFile(srcmgr.getMainFileID());

  FileID fileid = 
    srcmgr.createFileID(
      std::move(
        llvm::MemoryBuffer::getMemBufferCopy(
          llvm::StringRef((char*)s.ptr, s.len))),
      SrcMgr::C_User, 
      0, 0, new_loc);

  if (preprocessor.EnterSourceFile(fileid, nullptr, new_loc))
    return false;
  return true;
}

/* ----------------------------------------------------------------------------
 */
static void maybeResetParser(
    clang::Parser& parser,
    clang::Sema& sema)
{
  using namespace clang;

  ASTContext& astctx = sema.getASTContext();
  if (!astctx.getTranslationUnitDecl())
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
}

/* ----------------------------------------------------------------------------
 */
static void maybeSkipReplInputEndToken(clang::Parser& parser)
{
  if (parser.getCurToken().getKind() == clang::tok::annot_repl_input_end)
    parser.ConsumeAnyToken();
}

namespace clang{
// This class is declared a 'friend' of clang's Parser so that we may hook into
// it and call its private functions.
class LppParser
{
public:
  CompilerInstance& clang;
  Parser& parser;
  Context* ctx;

  LppParser(
        CompilerInstance& clang,
        Parser& parser,
        Context* ctx)
    : clang(clang),
      parser(parser),
      ctx(ctx) {}

  /* --------------------------------------------------------------------------
   */
  s64 getParseResultOffset(SourceManager& srcmgr)
  {
    const Token& curt = parser.getCurToken();
    if (curt.isOneOf(tok::eof, tok::annot_repl_input_end))
      return -1;
    else
      return getTokenStartOffset(srcmgr, curt);
  }

  /* --------------------------------------------------------------------------
   */
  ParseStmtResult parseStatement()
  {
    SourceManager&     srcmgr = clang.getSourceManager();
    DiagnosticsEngine& diags = clang.getDiagnostics();

    // Make sure diags is clean before we parse so we can detect any errors
    diags.Reset(true);

    StmtResult result = parser.ParseStatement();
    if (!result.isUsable())
      return {nullptr, 0};

    Stmt* stmt = result.get();

    if (!diags.hasErrorOccurred())
      return { (::Stmt*)stmt, getParseResultOffset(srcmgr) };

    // If an error has occured and this statement declared something, 
    // remove the declaration from the compiler's context to allow the 
    // user to try parsing the statement again if they fix it.

    if (auto declstmt = tryAs<DeclStmt>(stmt))
      parser.getCurScope()->getEntity()->
        removeDecl(declstmt->getSingleDecl());

    return {nullptr, 0};
  }

  /* --------------------------------------------------------------------------
   */
  const clang::Token consumeAndGetToken()
  {
    parser.ConsumeToken();
    return parser.getCurToken();
  }

  /* --------------------------------------------------------------------------
   */
  const SourceLocation consumeBrace()
  {
    return parser.ConsumeBrace();
  }

  /* --------------------------------------------------------------------------
   */
  const clang::Token consumeAndGetBraceToken()
  {
    consumeBrace();
    return parser.getCurToken();
  }

  /* --------------------------------------------------------------------------
   */
  Parser::DeclGroupPtrTy parseExternalDeclaration()
  {
    ParsedAttributes attrs(parser.getAttrFactory());
    ParsedAttributes declspecattrs(parser.getAttrFactory());
    return parser.ParseExternalDeclaration(attrs, declspecattrs);
  }

  /* --------------------------------------------------------------------------
   */
  ParseIdentifierResult parseIdentifier()
  {
    if (parser.expectIdentifier())
      return {nil, 0};

    const Token& tok = parser.getCurToken();
    SourceManager& srcmgr = clang.getSourceManager();
    FileID fileid = srcmgr.getFileID(tok.getLocation());

    // TODO(sushi) surely there is a better way to do this.
    auto beginoffset = srcmgr.getDecomposedLoc(tok.getLocation()).second;
    auto endoffset = srcmgr.getDecomposedLoc(tok.getEndLoc()).second;
    auto membuf = srcmgr.getBufferOrFake(fileid);
    auto buf = membuf.getBuffer().substr(beginoffset, endoffset-beginoffset);
    parser.ConsumeToken();
    return {{(u8*)buf.data(), buf.size()}, getParseResultOffset(srcmgr)};
  }

  /* --------------------------------------------------------------------------
   */
  Decl* lookupName(String s)
  {
    Preprocessor& preprocessor = clang.getPreprocessor();
    IdentifierInfo* ii = preprocessor.getIdentifierInfo(strRef(s));
    DeclarationName declname(ii);
    auto result = parser.getCurScope()->getEntity()->lookup(declname);
    if (result.empty())
      return nullptr;
    return result.front();
  }

  /* --------------------------------------------------------------------------
   */
  ParseTypeNameResult parseTypeName()
  {
    SourceManager&     srcmgr = clang.getSourceManager();
    DiagnosticsEngine& diags = clang.getDiagnostics();

    diags.Reset(true);
    
    auto TR = parser.ParseTypeName();
    if (!TR.isUsable() || diags.hasErrorOccurred())
      return {nullptr, 0};

    return {(::Type*)TR.get().getAsOpaquePtr(), getParseResultOffset(srcmgr)};
  }

  /* --------------------------------------------------------------------------
   */
  ParseDeclResult parseTopLevelDecl()
  {
    SourceManager&     srcmgr = clang.getSourceManager();
    DiagnosticsEngine& diags = clang.getDiagnostics();

    diags.Reset(true);

    Parser::DeclGroupPtrTy decl_group;
    Sema::ModuleImportState import_state;
    parser.ParseTopLevelDecl(decl_group, import_state);

    if (diags.hasErrorOccurred())
      return {nullptr, 0};

    return 
      {
        (::Decl*)decl_group.get().getSingleDecl(), 
        getParseResultOffset(srcmgr)
      };
  }

  /* --------------------------------------------------------------------------
   */
  ParseTopLevelDeclsResult parseTopLevelDecls()
  {
    SourceManager&     srcmgr = clang.getSourceManager();
    DiagnosticsEngine& diags = clang.getDiagnostics();
    ASTContext&        astctx = clang.getASTContext();

    diags.Reset(true);

    Parser::DeclGroupPtrTy decl_group;
    Sema::ModuleImportState import_state;

    INFO("parsing top level decls\n");

    std::vector<Decl*> decls;

    for (b8 eof = parser.ParseFirstTopLevelDecl(decl_group, import_state);
        !eof;
        eof = parser.ParseTopLevelDecl(decl_group, import_state))
    {
      auto ref = decl_group.get();
      if (ref.isSingleDecl())
      {
        decls.push_back(ref.getSingleDecl());
      }
      else
      {
        for (auto& decl : ref)
        {
          decls.push_back(decl);
        }
      }
    }

    if (diags.hasErrorOccurred())
    {
      // Similar to what we do in parsing statements, clear out any
      // declarations created here so that the user can try to fix 
      // stuff and parse again w/o clang complaining about stuff
      // being redefined.
      DeclGroupRef group = decl_group.get();
      for (auto decl = group.begin(); decl != group.end(); decl++)
        parser.getCurScope()->getEntity()->removeDecl(*decl);

      return {nullptr, 0};
    }

    if (decl_group.get().isNull())
      ERROR("decl group is null");

    auto group_ref = DeclGroupRef::Create(astctx, decls.data(), decls.size());

    auto dctx = ctx->allocate<DeclIterator>();
    dctx->initGroup(group_ref);

    astctx.getTranslationUnitDecl()->dump();

    return 
      {
        (DeclIter*)dctx,
        getParseResultOffset(srcmgr)
      };
  }

  /* --------------------------------------------------------------------------
   */
  ParseExprResult parseExpr()
  {
    SourceManager&     srcmgr = clang.getSourceManager();
    DiagnosticsEngine& diags = clang.getDiagnostics();

    diags.Reset(true);

    ExprResult result = parser.ParseExpression();
    if (!result.isUsable() || diags.hasErrorOccurred())
      return {nullptr, 0};

    return {(::Expr*)result.get(), getParseResultOffset(srcmgr)};
  }
};
}

/* ----------------------------------------------------------------------------
 */
Context* createContext(String* args, u64 argc)
{
  auto ctx = new Context;
  if (!ctx->init({args, argc}))
    return nullptr;
  return ctx;
}

/* ----------------------------------------------------------------------------
 */
void destroyContext(Context* ctx)
{
  ctx->deinit();
  delete ctx;
}

/* ----------------------------------------------------------------------------
 */
inline static clang::Decl* getClangDecl(Decl* decl)
{
  assert(decl);
  return (clang::Decl*)decl;
}

/* ----------------------------------------------------------------------------
 */
inline static clang::Stmt* getClangStmt(Stmt* stmt)
{
  return (clang::Stmt*)stmt;
}

/* ----------------------------------------------------------------------------
 */
inline static clang::QualType getClangType(Type* type)
{
  return clang::QualType::getFromOpaquePtr(type);
}

/* ----------------------------------------------------------------------------
 */
inline static const clang::TemplateArgument* getClangTemplateArgument(
    TemplateArg* arg)
{
  return (const clang::TemplateArgument*)arg;
}

/* ----------------------------------------------------------------------------
 */
Lexer* createLexer(Context* ctx, String s)
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

/* ----------------------------------------------------------------------------
 */
void destroyLexer(Context* ctx, Lexer* lexer)
{
  assert(ctx and lexer);

  delete lexer->lexer;
  ctx->lexers.remove(lexer->node);
}

/* ----------------------------------------------------------------------------
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

/* ----------------------------------------------------------------------------
 */
String tokenGetRaw(Context* ctx, Lexer* l, Token* t)
{
  assert(ctx);
  assert(l);
  assert(t);

  clang::SourceManager& srcmgr = ctx->clang->getSourceManager();
  auto tok = (clang::Token*)t;

  auto beginoffset = srcmgr.getDecomposedLoc(tok->getLocation()).second;
  auto endoffset = srcmgr.getDecomposedLoc(tok->getEndLoc()).second;
  auto membuf = srcmgr.getBufferOrFake(l->fileid);
  auto buf = membuf.getBuffer().substr(beginoffset, endoffset-beginoffset);
  return {(u8*)buf.data(), buf.size()};
}

/* ----------------------------------------------------------------------------
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

/* ----------------------------------------------------------------------------
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
  map(unknown, Whitespace); // just assume this is whitespace, kinda stupid 
                            // its marked this way
                // if this causes problems then maybe confirm it's whitespace 
                // by searching the raw later
                // TODO(sushi) if i actually do wind up forking clang so i 
                //             can have local patches maybe make this an 
                //             explicit token
  // TODO(sushi) MAYBE implement the rest of these, but not really neccessary 
  //             cause for punc and keyword we can just string compare in lua.

#undef map
  }
  return TK_Unhandled;
}

/* ----------------------------------------------------------------------------
 */
Decl* parseString(Context* ctx, String s)
{
  assert(ctx);

  using namespace clang;

  CompilerInstance& clang = *ctx->clang;
  Parser&           parser = *ctx->parser;
  SourceManager&    srcmgr = clang.getSourceManager();
  Preprocessor&     preprocessor = clang.getPreprocessor();
  Sema&             sema = clang.getSema();
  ASTContext&       astctx = sema.getASTContext();

  if (!startNewBuffer(srcmgr, preprocessor, s))
    return nullptr;

  maybeSkipReplInputEndToken(parser);

  Parser::DeclGroupPtrTy decl_group;
  Sema::ModuleImportState import_state;
  for (b8 at_eof = parser.ParseFirstTopLevelDecl(decl_group, import_state);
     !at_eof;
     at_eof = parser.ParseTopLevelDecl(decl_group, import_state))
  {/* dont do anything idk yet */}

  return (::Decl*)astctx.getTranslationUnitDecl();
}

/* ----------------------------------------------------------------------------
 */
b8 loadString(Context* ctx, String s)
{
  using namespace clang;

  assert(ctx);

  CompilerInstance& clang = *ctx->clang;
  Parser&           parser = *ctx->parser;
  SourceManager&    srcmgr = clang.getSourceManager();
  Preprocessor&     preprocessor = clang.getPreprocessor();
  Sema&             sema = clang.getSema();

  if (!startNewBuffer(srcmgr, preprocessor, s))
    return false;

  maybeSkipReplInputEndToken(parser);

  return true;
}

/* ----------------------------------------------------------------------------
 */
ParseStmtResult parseStatement(Context* ctx)
{
  assert(ctx);

  using namespace clang;

  CompilerInstance& clang = *ctx->clang;
  Parser&           parser = *ctx->parser;

  LppParser lppparser(clang, parser, ctx);
  return lppparser.parseStatement();
}

/* ----------------------------------------------------------------------------
 */
Decl* getStmtDecl(Context* ctx, Stmt* stmt)
{
  assert(ctx and stmt);
  if (auto decl = tryAs<clang::DeclStmt>(getClangStmt(stmt)))
    return (Decl*)decl->getSingleDecl();
  return nullptr;
}

/* ----------------------------------------------------------------------------
 */
ParseDeclResult parseTopLevelDecl(Context* ctx)
{
  assert(ctx);

  using namespace clang;

  CompilerInstance& clang = *ctx->clang;
  Parser&           parser = *ctx->parser;

  if (parser.getCurToken().getKind() == tok::annot_repl_input_end)
    parser.ConsumeAnyToken();

  LppParser lppparser(clang, parser, ctx);
  return lppparser.parseTopLevelDecl();
}

/* ----------------------------------------------------------------------------
 */
ParseTopLevelDeclsResult parseTopLevelDecls(Context* ctx)
{
  assert(ctx);

  using namespace clang;

  CompilerInstance& clang = *ctx->clang;
  Parser&           parser = *ctx->parser;

  LppParser lppparser(clang, parser, ctx);
  return lppparser.parseTopLevelDecls();
}

/* ----------------------------------------------------------------------------
 */
ParseExprResult parseExpr(Context* ctx)
{
  assert(ctx);

  using namespace clang;

  CompilerInstance& clang = *ctx->clang;
  Parser&           parser = *ctx->parser;

  LppParser lppparser(clang, parser, ctx);
  return lppparser.parseExpr();
}

/* ----------------------------------------------------------------------------
 */
ParseIdentifierResult parseIdentifier(Context* ctx)
{
  assert(ctx);

  using namespace clang;

  CompilerInstance& clang = *ctx->clang;
  Parser&           parser = *ctx->parser;

  LppParser lppparser(clang, parser, ctx);
  return lppparser.parseIdentifier();
}

/* ----------------------------------------------------------------------------
 */
ParseTypeNameResult parseTypeName(Context* ctx)
{
  assert(ctx);

  using namespace clang;

  CompilerInstance& clang = *ctx->clang;
  Parser&           parser = *ctx->parser;

  LppParser lppparser(clang, parser, ctx);
  return lppparser.parseTypeName();
}

/* ----------------------------------------------------------------------------
 */
Decl* lookupName(Context* ctx, String s)
{
  assert(ctx);

  using namespace clang;

  CompilerInstance& clang = *ctx->clang;
  Parser&           parser = *ctx->parser;

  LppParser lppparser(clang, parser, ctx);
  return (::Decl*)lppparser.lookupName(s);
}

/* ----------------------------------------------------------------------------
 */
void dumpDecl(Decl* decl)
{
  assert(decl);
  getClangDecl(decl)->dump();
}

/* ----------------------------------------------------------------------------
 */
void dumpAST(Context* ctx)
{
  using namespace clang;

  assert(ctx);

  CompilerInstance& clang = *ctx->clang;
  ASTContext&       astctx = clang.getASTContext();

  astctx.getTranslationUnitDecl()->dumpColor();
}

/* ----------------------------------------------------------------------------
 */
Decl* getTranslationUnitDecl(Context* ctx)
{
  return (Decl*)ctx->getASTContext()->getTranslationUnitDecl();
}

/* ----------------------------------------------------------------------------
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
  range->initContext(dctx);
  return (DeclIter*)range;
}

/* ----------------------------------------------------------------------------
 */
Decl* getNextDecl(DeclIter* iter)
{
  assert(iter);
  auto diter = ((DeclIterator*)iter);
  auto out = diter->next();
  if (!out)
    return nullptr;
  if (out->isImplicit()) // only really care about stuff thats actually 
                         // written here
    return getNextDecl(iter);
  return (Decl*)out;
}

/* ----------------------------------------------------------------------------
 */
DeclIter* getContextOfDecl(Context* ctx, Decl* decl)
{
  assert(ctx and decl);
  auto cdecl = getClangDecl(decl);
  auto dctx = clang::Decl::castToDeclContext(cdecl);
  auto range = ctx->allocate<DeclIterator>();
  range->initContext(dctx);
  return (DeclIter*)range;
}

/* ----------------------------------------------------------------------------
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
    kindcase(ClassTemplateSpecialization, Record); // mmm

#undef kindcase
  }

  return DeclKind_Unknown;
}

/* ----------------------------------------------------------------------------
 */
String getDeclName(Decl* decl)
{
  assert(decl);

  auto cdecl = getClangDecl(decl);
  if (!clang::NamedDecl::classof(cdecl))
    return {};

  auto ndecl = (clang::NamedDecl*)cdecl;
  llvm::StringRef name;
  if (ndecl->getDeclName().isIdentifier())
    name = ndecl->getName();
  else
    name = llvm::StringRef(ndecl->getNameAsString());

  return {(u8*)name.data(), name.size()};
}

/* ----------------------------------------------------------------------------
 */
Type* getDeclType(Decl* decl)
{
  auto cdecl = getClangDecl(decl);

  if (!clang::ValueDecl::classof(cdecl))
    return nullptr;

  auto vdecl = (clang::ValueDecl*)cdecl;
  return (Type*)vdecl->getType().getAsOpaquePtr();
}

/* ----------------------------------------------------------------------------
 */
String getClangDeclSpelling(Decl* decl)
{
  assert(decl);
  const char* s = getClangDecl(decl)->getDeclKindName();
  return {(u8*)s, strlen(s)};
}

/* ----------------------------------------------------------------------------
 */
u64 getDeclBegin(Context* ctx, Decl* decl)
{
  using namespace clang;

  CompilerInstance& clang = *ctx->clang;
  SourceManager&    srcmgr = clang.getSourceManager();

  auto [_, offset] = 
    srcmgr.getDecomposedSpellingLoc(
        getClangDecl(decl)->getBeginLoc());
  return offset;
}

/* ----------------------------------------------------------------------------
 */
u64 getDeclEnd(Context* ctx, Decl* decl)
{
  using namespace clang;

  CompilerInstance& clang = *ctx->clang;
  SourceManager&    srcmgr = clang.getSourceManager();

  auto [_, offset] = 
    srcmgr.getDecomposedSpellingLoc(
        getClangDecl(decl)->getEndLoc());
  return offset;
}

/* ----------------------------------------------------------------------------
 */
b8 isTypeDecl(Decl* decl)
{
  assert(decl);
  return clang::TypeDecl::classof(getClangDecl(decl));
}

/* ----------------------------------------------------------------------------
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

/* ----------------------------------------------------------------------------
 */
b8 isTagDecl(Decl* decl)
{
  return clang::TagDecl::classof(getClangDecl(decl));
}

/* ----------------------------------------------------------------------------
 */
static clang::TagDecl* getTagDecl(Decl* decl)
{
  assert(decl && isTagDecl(decl));
  return (clang::TagDecl*)getClangDecl(decl);
}

/* ----------------------------------------------------------------------------
 */
b8 isRecord(Decl* decl)
{
  return clang::RecordDecl::classof(getClangDecl(decl));
}

/* ----------------------------------------------------------------------------
 */
static clang::RecordDecl* getRecordDecl(Decl* decl)
{
  assert(decl && isRecord(decl));
  return (clang::RecordDecl*)getClangDecl(decl);
}

/* ----------------------------------------------------------------------------
 */
b8 isStruct(Decl* decl)
{
  assert(decl);
  auto cdecl = getClangDecl(decl);
  if (clang::TagDecl::classof(cdecl))
  {
    switch (((clang::TagDecl*)cdecl)->getTagKind())
    {
    case clang::TagTypeKind::Struct:
    case clang::TagTypeKind::Class:
      return true;
    }
  }
  return false;
}

/* ----------------------------------------------------------------------------
 */
b8 isUnion(Decl* decl)
{
  assert(decl);
  auto cdecl = getClangDecl(decl);
  if (clang::TagDecl::classof(cdecl))
    return clang::TagTypeKind::Union == ((clang::TagDecl*)cdecl)->getTagKind();
  return false;
}

/* ----------------------------------------------------------------------------
 */
b8 isEnum(Decl* decl)
{
  assert(decl);
  auto cdecl = getClangDecl(decl);
  if (clang::TagDecl::classof(cdecl))
    return clang::TagTypeKind::Enum == ((clang::TagDecl*)cdecl)->getTagKind();
  return false;
}

/* ----------------------------------------------------------------------------
 */
b8 isTypedef(Decl* decl)
{
  assert(decl);
  return clang::TypedefDecl::classof(getClangDecl(decl)) ||
         clang::TypeAliasDecl::classof(getClangDecl(decl));
}

/* ----------------------------------------------------------------------------
 */
BaseIter* createBaseIter(Context* ctx, Decl* decl)
{
  assert(decl);

  using namespace clang;

  auto* cdecl = getClangDecl(decl);
  if (auto* cxx = llvm::dyn_cast<CXXRecordDecl>(cdecl))
  {
    auto bases = cxx->bases();
    auto* iter = ctx->allocate<BaseIterator>();
    iter->current = bases.begin();
    iter->end = bases.end();
    return (BaseIter*)iter;
  }
  return nullptr;
}

/* ----------------------------------------------------------------------------
 */
Type* nextBase(BaseIter* iter)
{
  assert(iter);

  auto* iiter = (BaseIterator*)iter;

  return iiter->next();
}

/* ----------------------------------------------------------------------------
 */
b8 isTemplate(Decl* decl)
{
  assert(decl);
  return clang::ClassTemplateDecl::classof(getClangDecl(decl));
}

/* ----------------------------------------------------------------------------
 */
b8 isTemplateSpecialization(Decl* decl)
{
  assert(decl);
  return clang::ClassTemplateSpecializationDecl::classof(getClangDecl(decl));
}

/* ----------------------------------------------------------------------------
 */
Decl* getSpecializedDecl(Decl* decl)
{
  assert(isTemplateSpecialization(decl));
  auto tspec = (clang::ClassTemplateSpecializationDecl*)decl;
  return (Decl*)tspec->getSpecializedTemplate();
}

/* ----------------------------------------------------------------------------
 */
static clang::ClassTemplateSpecializationDecl* getTemplateSpecializationDecl(
    Decl* decl)
{
  assert(isTemplateSpecialization(decl));
  return (clang::ClassTemplateSpecializationDecl*)getClangDecl(decl);
}

/* ----------------------------------------------------------------------------
 */
TemplateArgIter* getTemplateArgIter(Context* ctx, Decl* decl)
{
  assert(decl);

  auto tdecl = getTemplateSpecializationDecl(decl);

  auto tarr = tdecl->getTemplateArgs().asArray();

  auto iter = ctx->allocate<TemplateArgIter>();
  iter->current = tarr.begin();
  iter->end = tarr.end();
  return iter;
}

/* ----------------------------------------------------------------------------
 */
TemplateArg* getNextTemplateArg(TemplateArgIter* iter)
{
  assert(iter);

  return (TemplateArg*)iter->next();
}

/* ----------------------------------------------------------------------------
 */
b8 isTemplateArgType(TemplateArg* arg)
{
  assert(arg);
  auto carg = getClangTemplateArgument(arg);
  return carg->getKind() == clang::TemplateArgument::ArgKind::Type;
}

/* ----------------------------------------------------------------------------
 */
Type* getTemplateArgType(TemplateArg* arg)
{
  assert(isTemplateArgType(arg));
  auto carg = getClangTemplateArgument(arg);
  return (Type*)carg->getAsType().getAsOpaquePtr();
}

/* ----------------------------------------------------------------------------
 */
b8 isTemplateArgIntegral(TemplateArg* arg)
{
  assert(arg);
  auto carg = getClangTemplateArgument(arg);
  return carg->getKind() == clang::TemplateArgument::ArgKind::Integral;
}

/* ----------------------------------------------------------------------------
 */
s64 getTemplateArgIntegral(TemplateArg* arg)
{
  assert(isTemplateArgIntegral(arg));
  auto carg = getClangTemplateArgument(arg);
  return carg->getAsIntegral().getExtValue();
}

/* ----------------------------------------------------------------------------
 */
b8 isNamespace(Decl* decl)
{
  assert(decl);
  return clang::NamespaceDecl::classof(getClangDecl(decl));
}

/* ----------------------------------------------------------------------------
 */
b8 isAnonymous(Decl* decl)
{
  assert(decl);
  auto* tag = getTagDecl(decl);
  return tag->getIdentifier() == nullptr &&
         tag->getTypedefNameForAnonDecl() == nullptr;
}

/* ----------------------------------------------------------------------------
 */
b8 isField(Decl* decl)
{
  assert(decl);
  return clang::FieldDecl::classof(getClangDecl(decl));
}

/* ----------------------------------------------------------------------------
 */
b8 isFunction(Decl* decl)
{
  assert(decl);
  return clang::FunctionDecl::classof(getClangDecl(decl));
}

/* ----------------------------------------------------------------------------
 */
static clang::FieldDecl* getFieldDecl(Decl* decl)
{
  assert(isField(decl));
  return (clang::FieldDecl*)getClangDecl(decl);
}

/* ----------------------------------------------------------------------------
 */
b8 isAnonymousField(Decl* decl)
{
  return getFieldDecl(decl)->isAnonymousStructOrUnion();
}

/* ----------------------------------------------------------------------------
 */
u64 getFieldOffset(Context* ctx, Decl* field)
{
  assert(ctx);
  return ctx->getASTContext()->getFieldOffset(getFieldDecl(field));
}

/* ----------------------------------------------------------------------------
 */
b8 isComplete(Decl* decl)
{
  assert(decl);
  auto* cdecl = getClangDecl(decl);
  assert(clang::TagDecl::classof(cdecl));
  return ((clang::TagDecl*)cdecl)->isCompleteDefinition();
}

/* ----------------------------------------------------------------------------
 */
Type* getTypedefSubType(Decl* decl)
{
  assert(isTypedef(decl));

  return 
    (Type*)((clang::TypedefDecl*)getClangDecl(decl))->
      getUnderlyingType().getAsOpaquePtr();
}

/* ----------------------------------------------------------------------------
 */
Decl* getTypedefSubDecl(Decl* decl)
{
  assert(isTypedef(decl));

  auto* subdecl = 
    ((clang::TypedefDecl*)getClangDecl(decl))->getUnderlyingDecl();

  if (subdecl == nullptr)
    return nullptr;

  return (Decl*)subdecl;
}

/* ----------------------------------------------------------------------------
 */
String getComment(Context* ctx, Decl* decl)
{
  assert(decl);

  using namespace clang;
  using namespace comments;

  auto* cdecl = getClangDecl(decl);

  ASTContext& ast = ctx->clang->getASTContext();
  Preprocessor& preprocessor = ctx->clang->getPreprocessor();
  SourceManager& srcmgr = ctx->clang->getSourceManager();

  if (const RawComment* comment = ast.getRawCommentForAnyRedecl(cdecl))
  {
    auto text = comment->getRawText(srcmgr);
    return 
      String::from((u8*)text.data(), text.size())
      .allocateCopy(&ctx->string_allocator);
  }

  return nil;
}

/* ----------------------------------------------------------------------------
 */
Decl* getDefinition(Decl* decl)
{
  assert(decl);
  auto* cdecl = getClangDecl(decl);
  if (!clang::TagDecl::classof(cdecl))
    return nullptr;
  return (Decl*)((clang::TagDecl*)cdecl)->getDefinition();
}

/* ----------------------------------------------------------------------------
 */
void makeComplete(Context* ctx, Type* type)
{
  assert(ctx && type);

  using namespace clang;

  CompilerInstance& clang = *ctx->clang;
  Sema& sema = clang.getSema();

  auto decl = getTypeDecl(type);
  if (decl == nullptr)
    return;

  if (isComplete(decl))
    return;

  auto ctype = getClangType(type);
  auto cdecl = getClangDecl(decl);

  sema.isCompleteType(SourceLocation(), ctype);
}

/* ----------------------------------------------------------------------------
 */
b8 isCanonicalDecl(Decl* decl)
{
  return getClangDecl(decl)->isCanonicalDecl();
}

/* ----------------------------------------------------------------------------
 */
Type* getFunctionReturnType(Decl* decl)
{
  assert(decl);
  
  auto cdecl = getClangDecl(decl);
  if (!clang::FunctionDecl::classof(cdecl))
    return nullptr;

  return (Type*)cdecl->getAsFunction()->getReturnType().getAsOpaquePtr();
}

/* ----------------------------------------------------------------------------
 */
b8 functionHasBody(Decl* decl)
{
  assert(decl);
  return getClangDecl(decl)->hasBody();
}

/* ----------------------------------------------------------------------------
 */
u32 getFunctionBodyBegin(Decl* decl)
{
  if (!functionHasBody(decl))
    return (u32)-1;
  return getClangDecl(decl)->getBody()->getBeginLoc().getRawEncoding();
}

/* ----------------------------------------------------------------------------
 */
u32 getFunctionBodyEnd(Decl* decl)
{
  if (!functionHasBody(decl))
    return (u32)-1;
  return getClangDecl(decl)->getBody()->getEndLoc().getRawEncoding();
}

/* ----------------------------------------------------------------------------
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

/* ----------------------------------------------------------------------------
 */
u32 getTagBodyBegin(Decl* decl)
{
  if (!tagHasBody(decl))
    return (u32)-1;
  auto tdecl = (clang::TagDecl*)getClangDecl(decl);
  return tdecl->getBraceRange().getBegin().getRawEncoding();
}

/* ----------------------------------------------------------------------------
 */
u32 getTagBodyEnd(Decl* decl)
{
  if (!tagHasBody(decl))
    return (u32)-1;
  auto tdecl = (clang::TagDecl*)getClangDecl(decl);
  return tdecl->getBraceRange().getEnd().getRawEncoding();
}

/* ----------------------------------------------------------------------------
 */
b8 tagIsEmbeddedInDeclarator(Decl* decl)
{
  auto cdecl = getClangDecl(decl);
  if (!clang::TagDecl::classof(cdecl))
    return false;
  return ((clang::TagDecl*)cdecl)->isEmbeddedInDeclarator();
}

/* ----------------------------------------------------------------------------
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

/* ----------------------------------------------------------------------------
 */
Decl* getNextParam(ParamIter* iter)
{
  assert(iter);
  return (Decl*)((ParamIterator*)iter)->next();
}

/* ----------------------------------------------------------------------------
 */
b8 isCanonical(Type* type)
{
  assert(type);
  return getClangType(type).isCanonical();
}

/* ----------------------------------------------------------------------------
 */
b8 isUnqualified(Type* type)
{
  assert(type);
  return !getClangType(type).hasQualifiers();
}

/* ----------------------------------------------------------------------------
 */
b8 isUnqualifiedAndCanonical(Type* type)
{
  assert(type);
  return isUnqualified(type) && isCanonical(type);
}

/* ----------------------------------------------------------------------------
 */
b8 isElaborated(Type* type)
{
  assert(type);
  auto ctype = getClangType(type);
  if (clang::ElaboratedType::classof(ctype.getTypePtr()))
    return true;
  return false;
}

/* ----------------------------------------------------------------------------
 */
Type* getDesugaredType(Context* ctx, Type* type)
{
  assert(ctx && type);

  return (Type*)
      getClangType(type).
      getDesugaredType(*ctx->getASTContext()).
      getAsOpaquePtr();
}

/* ----------------------------------------------------------------------------
 */
Type* getSingleStepDesugaredType(Context* ctx, Type* type)
{
  assert(ctx && type);

  return (Type*)
      getClangType(type).
      getSingleStepDesugaredType(*ctx->getASTContext()).
      getAsOpaquePtr();
}

/* ----------------------------------------------------------------------------
 */
b8 isTypedefType(Type* type)
{
  assert(type);
  return clang::TypedefType::classof(getClangType(type).getTypePtr());
}

/* ----------------------------------------------------------------------------
 */
Decl* getTypedefTypeDecl(Type* type)
{
  assert(isTypedefType(type));
  return 
    (Decl*)((clang::TypedefType*)getClangType(type).getTypePtr())->getDecl();
}

/* ----------------------------------------------------------------------------
 */
b8 isPointer(Type* type)
{
  assert(type);
  return getClangType(type)->isPointerType();
}

/* ----------------------------------------------------------------------------
 */
b8 isReference(Type* type)
{
  assert(type);
  return getClangType(type)->isReferenceType();
}

/* ----------------------------------------------------------------------------
 */
b8 isFunctionPointer(Type* type)
{
  assert(type);
  return getClangType(type)->isFunctionPointerType();
}

/* ----------------------------------------------------------------------------
 */
b8 isTemplateSpecializationType(Type* type)
{
  assert(type);
  return clang::TemplateSpecializationType::classof(
      getClangType(type).getTypePtr());
}

/* ----------------------------------------------------------------------------
 */
Type* getPointeeType(Type* type)
{
  assert(type && (isPointer(type) || isReference(type)));
  return (Type*)getClangType(type)->getPointeeType().getAsOpaquePtr();
}

/* ----------------------------------------------------------------------------
 */
b8 isArray(Type* type)
{
  assert(type);
  return getClangType(type)->isArrayType();
}

/* ----------------------------------------------------------------------------
 */
Type* getArrayElementType(Context* ctx, Type* type)
{
  assert(ctx && type && isArray(type));
  auto at = ctx->getASTContext()->getAsArrayType(getClangType(type));
  if (!at)
    return nullptr;
  return (Type*)at->getElementType().getAsOpaquePtr();
}

/* ----------------------------------------------------------------------------
 */
u64 getArrayLen(Context* ctx, Type* type)
{
  assert(ctx && type && isArray(type));
  auto at = ctx->getASTContext()->getAsArrayType(getClangType(type));
  if (!at)
    return -1;
  if (!clang::ConstantArrayType::classof(at))
    return -1;

  // yeah idk this will probably break at some point 
  return *((clang::ConstantArrayType*)at)->getSize().getRawData();
}

/* ----------------------------------------------------------------------------
 */
Type* getCanonicalType(Type* type)
{
  assert(type);
  return (Type*)getClangType(type).getCanonicalType().getAsOpaquePtr();
}

/* ----------------------------------------------------------------------------
 */
Type* getUnqualifiedType(Type* type)
{
  assert(type);
  return (Type*)getClangType(type).getUnqualifiedType().getAsOpaquePtr();
}

/* ----------------------------------------------------------------------------
 */
Type* getUnqualifiedCanonicalType(Type* type)
{
  assert(type);
  return getUnqualifiedType(getCanonicalType(type));
}

/* ----------------------------------------------------------------------------
 */
String getTypeName(Context* ctx, Type* type)
{
  assert(ctx && type);

  auto ctype = getClangType(type);

  // yeah this kiiiinda sucks.
  // This whole process looks like it takes 3 copies. Maybe there's
  // someway to just pass clang a buffer to write into rather than
  // extracting it from a std::string like if we implement 
  // llvm::raw_ostream or something.
  //
  // TODO(sushi) raw_ostream is now implemented so fix this sometime thanks ^_^
  auto stdstr = ctype.getAsString();
  io::Memory m;
  m.open(stdstr.size(), &ctx->allocator);
  m.write({(u8*)stdstr.data(), stdstr.size()});

  return m.asStr();
}

/* ----------------------------------------------------------------------------
 */
String getCanonicalTypeName(Context* ctx, Type* type)
{
  assert(ctx && type);
  return getTypeName(ctx, getCanonicalType(type));
}

/* ----------------------------------------------------------------------------
 */
String getUnqualifiedTypeName(Context* ctx, Type* type)
{
  assert(ctx && type);
  return getTypeName(ctx, getUnqualifiedType(type));
}

/* ----------------------------------------------------------------------------
 */
String getUnqualifiedCanonicalTypeName(Context* ctx, Type* type)
{
  assert(ctx && type);
  return getTypeName(ctx, getUnqualifiedCanonicalType(type));
}

/* ----------------------------------------------------------------------------
 */
u64 getTypeSize(Context* ctx, Type* type)
{
  assert(ctx && type);
  
  auto* astctx = ctx->getASTContext();
  if (astctx == nullptr)
  {
    printf("failed to get ast context\n");
    return 0;
  }

  const auto& typeinfo = astctx->getTypeInfo(getClangType(type));

  return typeinfo.Width;
}

/* ----------------------------------------------------------------------------
 */
b8 typeIsBuiltin(Type* type)
{
  assert(type);
  return getClangType(type)->getTypeClass() == ClangTypeClass::Builtin;
}

/* ----------------------------------------------------------------------------
 */
Decl* getTypeDecl(Type* type)
{
  assert(type);

  auto ctype = getClangType(type);

  if (clang::LocInfoType::classof(ctype.getTypePtr()))
    ctype = ((clang::LocInfoType*)ctype.getTypePtr())->getType();

  auto unsugared = ctype->getUnqualifiedDesugaredType();

  // TODO(sushi) not sure if any other types have decls
  if (!clang::TagType::classof(unsugared))
    return nullptr;

  return (Decl*)unsugared->getAsTagDecl();
}

/* ----------------------------------------------------------------------------
 */
void dumpType(Type* type)
{
  assert(type);
  getClangType(type)->dump();
}

/* ----------------------------------------------------------------------------
 */
FieldIter* createFieldIter(Context* ctx, Decl* decl)
{
  assert(decl);
  auto cdecl = getClangDecl(decl);
  if (clang::RecordDecl::classof(cdecl))
  {
    auto rdecl = (clang::RecordDecl*)cdecl;
    if (rdecl->field_empty())
      return nullptr;
    auto iter = ctx->allocate<FieldIterator>();
    iter->current = rdecl->field_begin();
    iter->end = rdecl->field_end();
    return (FieldIter*)iter;
  }
  return nullptr;
}

/* ----------------------------------------------------------------------------
 */
Decl* getNextField(FieldIter* iter)
{
  assert(iter);
  return (Decl*)((FieldIterator*)iter)->next();
}

/* ----------------------------------------------------------------------------
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

/* ----------------------------------------------------------------------------
 */
Decl* getNextEnum(EnumIter* iter)
{
  assert(iter);
  return (Decl*)((EnumIterator*)iter)->next();
}

/* ----------------------------------------------------------------------------
 */
s64 getEnumValue(Decl* decl)
{
  assert(decl && clang::EnumConstantDecl::classof(getClangDecl(decl)));

  auto* edecl = (clang::EnumConstantDecl*)getClangDecl(decl);
  auto val = edecl->getInitVal();
  u64 extval = val.getExtValue();
  return *(s64*)&extval;
}

/* ----------------------------------------------------------------------------
 */
Type* lookupType(Context* ctx, String name)
{
  assert(ctx && notnil(name));

  using namespace clang;

  CompilerInstance& clang = *ctx->clang;
  Parser&           parser = *ctx->parser;
  SourceManager&    srcmgr = clang.getSourceManager();
  Preprocessor&     preprocessor = clang.getPreprocessor();
  Sema&             sema = clang.getSema();
  ASTContext&       astctx = sema.getASTContext();

  loadString(ctx, name);

  LppParser lppparser(clang, parser, ctx);
  ParseTypeNameResult result = lppparser.parseTypeName();

  return (::Type*)result.type;
}

/* ----------------------------------------------------------------------------
 */
// I dont really care for doing this but I dont feel like figuring out their
// error thing right now so whatever.
llvm::ExitOnError exit_on_error;
void addIncludeDir(Context* ctx, String s)
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

/* ----------------------------------------------------------------------------
 */
String getDependencies(String filename, String file, String* args, u64 argc)
{
  using namespace clang;
  using namespace clang::tooling::dependencies;

  filename = filename.subFromLast('/').sub(1);
  std::string filenamestr((char*)filename.ptr, filename.len);
  filenamestr += ".cpp";

  std::vector<std::string> compilation = 
  { 
    ENOSI_CLANG_EXECUTABLE,
    "-c",
    filenamestr,
  };

  for (u64 i = 0; i < argc; ++i)
  {
    compilation.push_back(std::string(
          (char*)args[i].ptr, (char*)args[i].ptr + args[i].len));
  }

  auto ofs = new llvm::vfs::OverlayFileSystem(llvm::vfs::getRealFileSystem());
  auto vfs = new llvm::vfs::InMemoryFileSystem;
  ofs->pushOverlay(vfs);

  auto cwd_path = iro::fs::Path::cwd();
  defer { cwd_path.destroy(); };

  llvm::StringRef cwd((char*)cwd_path.buffer.ptr, cwd_path.buffer.len);

  vfs->addFile(
      llvm::Twine(cwd) + "/" + filenamestr, 
      0,
      llvm::MemoryBuffer::getMemBuffer(
        llvm::StringRef((char*)file.ptr, file.len)));

  DependencyScanningService service(
      ScanningMode::DependencyDirectivesScan,
      ScanningOutputFormat::Full);
  DependencyScanningTool tool(service, ofs);

  llvm::ExitOnError exitOnErr;
  auto result = exitOnErr(tool.getDependencyFile(compilation, cwd));

  String out;
  out.ptr = (u8*)mem::stl_allocator.allocate(result.size());
  mem::copy(out.ptr, result.data(), result.size());
  out.len = result.size();

  return out;
}

/* ----------------------------------------------------------------------------
 */
void destroyDependencies(String deps)
{
  mem::stl_allocator.free(deps.ptr);
}

/* ----------------------------------------------------------------------------
 */
b8 beginNamespace(Context* ctx, String name)
{
  using namespace clang;

  CompilerInstance& clang = *ctx->clang;
  Parser&           parser = *ctx->parser;
  Sema&             sema = clang.getSema();
  SourceManager&    srcmgr = clang.getSourceManager();
  Preprocessor&     preprocessor = clang.getPreprocessor();

  // create a fake buffer containing the namespace declaration.
  io::Memory buffer;
  if (!buffer.open(&ctx->string_allocator))
    return ERROR("failed to open buffer for fake namespace decl\n");    
  io::formatv(&buffer, "namespace ", name, " {\n");

  if (!startNewBuffer(srcmgr, preprocessor, buffer.asStr()))
    return ERROR("failed to upload fake namespace decl buffer to clang\n");

  parser.ConsumeAnyToken();
  clang::Token ns_tok = parser.getCurToken();

  parser.ConsumeToken();
  clang::Token id_tok = parser.getCurToken();

  parser.ConsumeToken();
  clang::Token lb_tok = parser.getCurToken();

  auto ns_scope = new Parser::ParseScope(&parser, Scope::DeclScope);
  ctx->parse_scopes.pushTail(ns_scope);

  ParsedAttributes attrs(parser.getAttrFactory());

  UsingDirectiveDecl* implicit_using = nullptr;
  clang::Decl* namespace_decl = 
    parser.getActions().ActOnStartNamespaceDef(
      parser.getCurScope(),
      SourceLocation(),
      ns_tok.getLocation(),
      id_tok.getLocation(),
      id_tok.getIdentifierInfo(),
      lb_tok.getLocation(),
      attrs,
      implicit_using,
      !ctx->namespace_decls.isEmpty());

  ctx->namespace_decls.pushTail(namespace_decl);

  parser.ConsumeAnyToken();

  return true;
}

/* ----------------------------------------------------------------------------
 */
void endNamespace(Context* ctx)
{
  using namespace clang;

  CompilerInstance& clang = *ctx->clang;
  Parser&           parser = *ctx->parser;
  Sema&             sema = clang.getSema();
  SourceManager&    srcmgr = clang.getSourceManager();
  Preprocessor&     preprocessor = clang.getPreprocessor();

  Parser::ParseScope* ns_scope = ctx->parse_scopes.popTail();
  delete ns_scope;

  static const String endstr = "}\n"_str;

  startNewBuffer(srcmgr, preprocessor, endstr);
  parser.ConsumeAnyToken();

  auto namespace_decl = ctx->namespace_decls.popTail();

  INFO("is: ", tok::getTokenName(parser.getCurToken().getKind()), "\n");

  parser.getActions().ActOnFinishNamespaceDef(
      namespace_decl,
      parser.getCurToken().getLocation());

  parser.ConsumeAnyToken();
}

/* ----------------------------------------------------------------------------
 *  Miscellaneous experimenting with clang directly.
 */
void playground()
{
  using namespace clang;
  
  auto ctx = createContext(nullptr, 0);
  if (!ctx)
    return;
  defer { destroyContext(ctx); };

  CompilerInstance& clang = *ctx->clang;
  Parser&           parser = *ctx->parser;
  Sema&             sema = clang.getSema();
  SourceManager&    srcmgr = clang.getSourceManager();
  Preprocessor&     preprocessor = clang.getPreprocessor();
  ASTContext&       astctx = clang.getASTContext();

  LppParser lppparser(clang, parser, ctx);

  beginNamespace(ctx, "Test"_str);

  loadString(ctx, R"cpp(
    int test()
    {
      return 1 + 2;
    }
  )cpp"_str);

  parser.ConsumeAnyToken();

  lppparser.parseExternalDeclaration();

  endNamespace(ctx);

  parseString(ctx, R"cpp(
    int main()
    {
      test();
    }
  )cpp"_str);


#if 0 

  maybeResetParser(parser, sema);
  startNewBuffer(srcmgr, preprocessor,
      "namespace Test {\n"_str);

  parser.ConsumeAnyToken();
  clang::Token ns_tok = parser.getCurToken();
  
  INFO("is: ", tok::getTokenName(parser.getCurToken().getKind()), "\n");

  parser.ConsumeToken();
  clang::Token id_tok = parser.getCurToken();

  INFO("is: ", tok::getTokenName(parser.getCurToken().getKind()), "\n");

  parser.ConsumeToken();
  clang::Token lb_tok = parser.getCurToken();

  INFO("is: ", tok::getTokenName(parser.getCurToken().getKind()), "\n");

  Parser::ParseScope ns_scope(&parser, Scope::DeclScope);

  ParsedAttributes attrs(parser.getAttrFactory());

  IdentifierInfo* ii = id_tok.getIdentifierInfo();

  UsingDirectiveDecl* implicit_using = nullptr;
  clang::Decl* namespace_decl = 
    parser.getActions().ActOnStartNamespaceDef(
        parser.getCurScope(), 
        SourceLocation(), 
        ns_tok.getLocation(),
        id_tok.getLocation(),
        id_tok.getIdentifierInfo(),
        lb_tok.getLocation(),
        attrs,
        implicit_using,
        false);
  
  startNewBuffer(srcmgr, preprocessor,
      "void test() {}"_str);
  parser.ConsumeAnyToken();

  lppparser.parseExternalDeclaration();

  ns_scope.Exit();

  startNewBuffer(srcmgr, preprocessor,
      "}\n"_str);
  parser.ConsumeAnyToken();

  parser.getActions().ActOnFinishNamespaceDef(
      namespace_decl,
      lppparser.consumeBrace());

  namespace_decl->dump();
  
  startNewBuffer(srcmgr, preprocessor, R"cpp(
      int main()
      {
        Test::test();
      }
    )cpp"_str);

  maybeResetParser(parser, sema);

  Parser::DeclGroupPtrTy decl_group;
  Sema::ModuleImportState import_state;
  for (b8 at_eof = parser.ParseFirstTopLevelDecl(decl_group, import_state);
       !at_eof;
       at_eof = parser.ParseTopLevelDecl(decl_group, import_state))
  {/* dont do anything idk yet */}

  astctx.getTranslationUnitDecl()->dump();
#endif
}

