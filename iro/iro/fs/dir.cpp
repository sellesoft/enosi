#include "dir.h"
#include "../platform.h"

namespace iro::fs
{

/* ------------------------------------------------------------------------------------------------ Dir::open
 */
Dir Dir::open(str path)
{
	Dir out = {};

	if (!platform::opendir(&out.handle, path))
		return nil;

	return out;
}

Dir Dir::open(Path path)
{
	return open(path.buffer.asStr());
}

Dir Dir::open(File::Handle file_handle)
{
	Dir out = {};
	
	if (!platform::opendir(&out.handle, file_handle))
		return nil;

	return out;
}

/* ------------------------------------------------------------------------------------------------ Dir::close
 */
void Dir::close()
{
	assert(platform::closedir(handle));
}

/* ------------------------------------------------------------------------------------------------ Dir::next
 */
s64 Dir::next(Bytes buffer)
{
	return platform::readdir(handle, buffer);
}

/* ------------------------------------------------------------------------------------------------ Dir::make
 */
b8 Dir::make(str path, b8 make_parents)
{
	return platform::makeDir(path, make_parents);
}

b8 Dir::make(Path path, b8 make_parents)
{
	return make(path.buffer.asStr(), make_parents);
}

}
