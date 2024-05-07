#include "common.h"
#include "io/io.h"
#include "fs/fs.h"
#include "time/time.h"
#include "logger.h"

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

int main()
{
	if (!log.init())
		return 1;

	fs::File out = fs::File::fromStdout();	
	{
		using enum Log::Dest::Flag;

		log.newDestination("stdout"_str, &out, 
				Log::Dest::Flags::from(
					ShowCategoryName, 
					ShowVerbosity, 
					AllowColor));
	}



	fs::File testfile;
	testfile.open("src/test.cpp"_str, fs::OpenFlags::from(fs::OpenFlag::Read));

	io::Memory buffer;
	buffer.open();

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

	return 0;

	Decl* tu = getTranslationUnitDecl(ctx);
	DeclIter* toplevel = createDeclIter(ctx, tu);
	while (Decl* d = getNextDecl(toplevel))
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

		}
	}
	return 0;
}

