#include "lake.h"
#include "stdlib.h"

#include "iro/logger.h"
#include "iro/fs/fs.h"
#include "iro/fs/glob.h"
#include "iro/fs/path.h"

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
	defer { iro::log.deinit(); };

	auto f = fs::File::from("temp/log"_str, fs::OpenFlag::Create | fs::OpenFlag::Truncate | fs::OpenFlag::Write);

	{
		using enum Log::Dest::Flag;
		iro::log.newDestination("stdout"_str, &fs::stdout, 
				AllowColor | 
				// ShowCategoryName |
				ShowVerbosity |
				PadVerbosity |
				TrackLongestName);
		iro::log.newDestination("templog"_str, &f, {});
	}

	if (!lake.init(argv, argc))
		return 1;
	defer { lake.deinit(); };

	if (!lake.run())
		return 1;
	return 0;
}
