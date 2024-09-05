#include "iro/common.h"
#include "iro/io/io.h"
#include "iro/fs/fs.h"
#include "iro/time/time.h"
#include "iro/logger.h"

#include "lppclang.h"

#include "stdio.h"

using namespace iro;

#define DEFINE_GDB_PY_SCRIPT(script_name) \
  asm("\
.pushsection \".debug_gdb_scripts\", \"MS\",@progbits,1\n\
.byte 1 /* Python */\n\
.asciz \"" script_name "\"\n\
.popsection \n\
");

DEFINE_GDB_PY_SCRIPT("lpp-gdb.py");

static Logger logger = 
  Logger::create("main"_str, Logger::Verbosity::Trace);

void printEnum(Context* ctx, Decl* decl)
{
  INFO("enum: ", getDeclName(decl), "\n");
  SCOPED_INDENT;
  
  EnumIter* iter = createEnumIter(ctx, decl);
  while (Decl* decl = getNextEnum(iter))
  {
    INFO(getDeclName(decl), "\n");
  }
}

void printRecord(Context* ctx, Decl* decl)
{
  INFO("record: ", getDeclName(decl), "\n");
  SCOPED_INDENT;

  if (DeclIter* iter = createDeclIter(ctx, decl))
  {
    while (Decl* elem = getNextDecl(iter))
    {
      switch (getDeclKind(elem))
      {

      case DeclKind_Record:
        printRecord(ctx, elem);
        break;

      case DeclKind_Field:
        {
          INFO("field: '", getDeclName(elem), "' ", getTypeName(ctx, getDeclType(elem)), "\n");
        }
        break;

      case DeclKind_Enum:
        printEnum(ctx, elem);
        break;

      default:
        INFO("unknown decl kind: ", getClangDeclSpelling(elem), "\n");
        break;
      }
    }
  }

  if (tagHasBody(decl))
  {
    INFO("defined from ", getTagBodyBegin(decl), " to ", getTagBodyEnd(decl), "\n");
  }
}

void printTranslationUnitDecls(Context* ctx, DeclIter* iter)
{
  while (Decl* d = getNextDecl(iter))
  {
    switch (getDeclKind(d))
    {

      case DeclKind_Record:
        printRecord(ctx, d);
        break;

      case DeclKind_Function:
        {
          INFO("func: ", getDeclName(d), "\n");
          SCOPED_INDENT;
          INFO("return type: ", getTypeName(ctx, getFunctionReturnType(d)), "\n");
          if (ParamIter* iter = createParamIter(ctx, d))
          {
            INFO("params: \n");
            SCOPED_INDENT;
            while (Decl* param = getNextParam(iter))
            {
              INFO(getDeclName(param), ": ", getTypeName(ctx, getDeclType(param)), "\n");
            }
          }

          if (functionHasBody(d))
          {
            INFO("defined from ", getFunctionBodyBegin(d), " to ", getFunctionBodyEnd(d), "\n");
          }
        }
        break;

      case DeclKind_Enum:
        {
          INFO("enum: ", getDeclName(d), "\n");
          SCOPED_INDENT;
          INFO("elements:\n");
          SCOPED_INDENT;
          EnumIter* iter = createEnumIter(ctx, d);
          while (Decl* e = getNextEnum(iter))
          {
            INFO(getDeclName(e), "\n");
          }
        }
        break;

      case DeclKind_Variable:
        {
          INFO("variable: ", getDeclName(d), "\n");
          SCOPED_INDENT;
        }
        break;
    }
  }

}

int testContext()
{
  auto testfile = fs::File::from("src/test.cpp"_str, fs::OpenFlag::Read);
  if (isnil(testfile))
    return 1;
  defer { testfile.close(); };

  io::Memory buffer;
  buffer.open();
  defer { buffer.close(); };

  for (;;)
  {
    buffer.reserve(32);
    u64 bytes_read = testfile.read({buffer.buffer + buffer.len, 32});
    if (!bytes_read)
      break;
    buffer.commit(bytes_read);
  }


  Context* ctx = createContext(nullptr, 0);
  defer { destroyContext(ctx); };

  if (!createASTFromString(ctx, buffer.asStr()))
    return 1;

  Decl* tu = getTranslationUnitDecl(ctx);
  DeclIter* toplevel = createDeclIter(ctx, tu);
  printTranslationUnitDecls(ctx, toplevel);

  return 0;
}

int testLexer()
{
  Context* ctx = createContext(nullptr, 0);
  if (!ctx)
    return 1;
  defer { destroyContext(ctx); };

  Lexer* lexer = createLexer(ctx, R"cpp(
    int main()
    {
      enum class State 
      {
        Walking,
        Running,
      } state;
    }
  )cpp"_str);
  
  while (Token* t = lexerNextToken(lexer))
  {
    INFO(tokenGetRaw(ctx, lexer, t), "\n");
    tokenGetKind(t);
  }

  return 0;
}

int testLookup()
{
  Context* ctx = createContext(nullptr, 0);
  if (!ctx)
    return 1;
  defer { destroyContext(ctx); };

  parseString(ctx, R"(
    template<typename T>
    struct Apple
    {
      int leaves;
      float nutrients;
      T thing;
    };

    int main() 
    {
      int a = 1;
    }
  )"_str);

  parseString(ctx, R"cpp(
    int b;
  )cpp"_str);

  
  Type* type = lookupType(ctx, "Apple<int>"_str);
  
  str name = getTypeName(ctx, type);

  printf("%s\n", name.bytes);
  
  return 0;
}

int testMultipleParses()
{
  Context* ctx = createContext(nullptr, 0);
  if (!ctx)
    return 1;
  defer { destroyContext(ctx); };

  Decl* d;

  d = parseString(ctx, R"(
    #define DEBUG false


    struct Movement
    {
      float pos[3];
      float vel[3];
    };

    struct Player
    {
      Movement movement;
      int health;
    };
  )"_str);

  dumpDecl(d);

  d = parseString(ctx, R"(
    int main()
    {
      #if DEBUG
      Player p;
      p.movement.pos[0] = 1.f;
      p.movement.pos[1] = 2.f;
      p.movement.pos[2] = 3.f;
      p.movement.vel = {};
      p.health = 100;
      #endif
    }
  )"_str);

  dumpDecl(d);

  return 0;
}

int testParseStmt()
{
  Context* ctx = createContext(nullptr, 0);
  if (!ctx)
    return 1;
  defer { destroyContext(ctx); };

  str test = R"cpp(
    int a = 1 + 2;
    int b = 3 + 4;
    int c = a + b;
  )cpp"_str;

  loadString(ctx, test);

  ParseStmtResult result;
  u64 offset = 0;

  auto tryParse = [&]()
  {
    INFO("~~~~~~~~~~~~~~~~~~~~~~~~~~\n");

    result = parseStatement(ctx);
    
    if (result.stmt == nullptr)
    {
      INFO("parse failed\n");
      return false;
    }

    str s = {test.bytes+offset};
    if (result.offset == -1)
      s.len = test.bytes  + test.len - s.bytes;
    else
      s.len = result.offset - offset;

    INFO("parsed statement: ", s, "\n");

    offset = result.offset;
    if (offset == -1)
      return false;
    return true;
  };

  while (tryParse())
    ;

  return 0;
}

int testParsePartialStmt()
{
  Context* ctx = createContext(nullptr, 0);
  if (!ctx)
    return 1;
  defer { destroyContext(ctx); };

  io::Memory mem;
  mem.open();

  mem.write(R"cpp(
    int a = 1 + 2;
    int b = 
  )cpp"_str);

  loadString(ctx, mem.asStr());

  s64 offset = 0;

  ParseStmtResult result;

  result = parseStatement(ctx);
  assert(result.stmt);

  INFO("parsed statement: ", 
      str{mem.buffer+offset, u64(result.offset-offset)}, "\n");

  offset = result.offset;

  // should fail.
  result = parseStatement(ctx);
  assert(!result.stmt);

  s64 newstart = mem.len;
  mem.write(" a + 2; "_str);

  loadString(ctx, str{mem.buffer+offset, u64(mem.len-offset)});

  result = parseStatement(ctx);
  assert(result.stmt);

  INFO("parsed statement: ", 
      str{mem.buffer+offset, u64(mem.len-offset)}, "\n");

  return 0;
}

int testLookupName()
{
  Context* ctx = createContext(nullptr, 0);
  if (!ctx)
    return 1;
  defer { destroyContext(ctx); };

  str test = R"cpp(
    int a = 1 + 2;
    int b = 2 + 3;
  )cpp"_str;

  loadString(ctx, test);

  ParseStmtResult result;

  result = parseStatement(ctx);
  assert(result.stmt);
  result = parseStatement(ctx);
  assert(result.stmt);

  Decl* lookup = lookupName(ctx, "a"_str);
  dumpDecl(lookup);

  loadString(ctx, R"cpp(
    struct Apple 
    {
      int leaves;
    };
  )cpp"_str);

  parseTopLevelDecl(ctx);

  loadString(ctx, R"cpp(
    auto apple = Apple{1};
  )cpp"_str);

  result = parseStatement(ctx);

  if (auto decl = getStmtDecl(ctx, result.stmt))
  {
    dumpDecl(decl);
  }

  return 0;
}

int testArgs()
{
  str args[] =
  {
    "-I/home/sushi/src/enosi/iro"_str,
    "-DHELLO"_str,
  };

  auto ctx = createContext(args, 1);
  if (!ctx)
    return 1;
  defer { destroyContext(ctx); };

  loadString(ctx, R"cpp(
    #include "iro/containers/array.h"

    int main()
    {
      Array<int> arr;
      arr.hello();
      return 1;
    }
  )cpp"_str);

  auto result = parseTopLevelDecls(ctx);

  while (auto decl = getNextDecl(result.iter))
  {
    dumpDecl(decl);
  }

  return 0;
}

int testDepedencies()
{
  str args[] = 
  {
    "-Iiro"_str,
  };

  getDependencies(R"cpp(
    #include "iro/containers/array.h"

    int main()
    {
      Array<int> array;
    }
  )cpp"_str, args, 1);

  return 0;
}

int main()
{

  if (!log.init())
    return 1;
  defer { log.deinit(); };

  {
    using enum Log::Dest::Flag;

    log.newDestination("stdout"_str, &fs::stdout, 
        Log::Dest::Flags::from(
          ShowCategoryName, 
          ShowVerbosity, 
          AllowColor));
  }

  // return testContext();
  // return testLexer();
  // return testLookup();
  // return testMultipleParses();
  // return testParseStmt();
  // return testParsePartialStmt();
  // return testLookupName();
  // return testArgs();
  return testDepedencies();
}

