#include "lake.h"
#include "stdlib.h"

#include "logger.h"
#include "fs/fs.h"

using namespace iro;

int main(const int argc, const char* argv[]) 
{
	if (!iro::log.init())
		return 1;

	fs::File stdout = fs::File::stdout();
	{
		using enum Log::Dest::Flag;
		iro::log.newDestination("stdout"_str, &stdout, 
				AllowColor | 
				ShowCategoryName |
				ShowVerbosity);
	}

	if (!lake.init(argv, argc))
		return 1;
	if (!lake.run())
		return 1;
	return 0;
}
