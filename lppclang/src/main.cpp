#include "iro/common.h"
#include "iro/io/io.h"
#include "iro/fs/fs.h"
#include "iro/time/time.h"
#include "iro/logger.h"

#include "lppclang.h"

using namespace iro;

#define DEFINE_GDB_PY_SCRIPT(script_name) \
  asm("\
.pushsection \".debug_gdb_scripts\", \"MS\",@progbits,1\n\
.byte 1 /* Python */\n\
.asciz \"" script_name "\"\n\
.popsection \n\
");

DEFINE_GDB_PY_SCRIPT("lpp-gdb.py");

static Logger logger = Logger::create("main"_str, Logger::Verbosity::Trace);

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


	Context* ctx = createContext();
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
	Context* ctx = createContext();
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
	Context* ctx = createContext();
	if (!ctx)
		return 1;
	defer { destroyContext(ctx); };

	parseString(ctx, R"(
		struct Apple
		{
			int leaves;
			float nutrients;
		};

		int main() 
		{
			int a;
		
	)"_str);

	
	Type* type = lookupType(ctx, "a"_str);

	INFO(getTypeName(ctx, type));
	
	return 0;
}

int testMultipleParses()
{
	Context* ctx = createContext();
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
	return testLookup();
	// return testMultipleParses();
}

