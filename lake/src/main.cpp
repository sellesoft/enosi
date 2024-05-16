#include "lake.h"
#include "stdlib.h"

#include "logger.h"
#include "fs/fs.h"

using namespace iro;

#define DEFINE_GDB_PY_SCRIPT(script_name) \
  asm("\
.pushsection \".debug_gdb_scripts\", \"MS\",@progbits,1\n\
.byte 1 /* Python */\n\
.asciz \"" script_name "\"\n\
.popsection \n\
");

DEFINE_GDB_PY_SCRIPT("lpp-gdb.py");

int main(const int argc, const char* argv[]) 
{
	if (!iro::log.init())
		return 1;

	fs::File stdout = fs::File::stdout();
	defer { stdout.close(); };
	{
		using enum Log::Dest::Flag;
		iro::log.newDestination("stdout"_str, &stdout, 
				AllowColor | 
				ShowCategoryName |
				ShowVerbosity |
				PadVerbosity);
	}

	if (!lake.init(argv, argc))
		return 1;
	defer { lake.deinit(); };

	if (!lake.run())
		return 1;
	return 0;
}
