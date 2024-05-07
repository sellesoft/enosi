#include "fs.h"
#include "platform.h"

namespace iro::fs
{

/* ------------------------------------------------------------------------------------------------ File::open
 */
b8 File::open(OpenFlags flags)
{
	if (isOpen())
		return false;

	if (path.isempty())
		return false;

	if (!platform::open(&handle, path, flags))
		return false;

	setOpen();

	if (flags.test(OpenFlag::Read))
		setReadable();

	if (flags.test(OpenFlag::Write))
		setWritable();

	return true;
}

b8 File::open(str path, OpenFlags flags)
{
	if (isOpen())
		return false;

	this->path = path;

	return open(flags);
}

/* ------------------------------------------------------------------------------------------------ File::close
 */
void File::close()
{
	if (!isOpen())
		return;

	platform::close(handle);
	
	unsetOpen();
	unsetReadable();
	unsetWritable();
}

/* ------------------------------------------------------------------------------------------------ File::write
 */
s64 File::write(Bytes bytes)
{
	if (!isOpen() ||
		!canWrite())
		return -1;

	return platform::write(handle, bytes);
}

/* ------------------------------------------------------------------------------------------------ File::read
 */
s64 File::read(Bytes bytes)
{
	if (!isOpen() ||
		!canRead())
		return -1;

	return platform::read(handle, bytes);
}

/* ------------------------------------------------------------------------------------------------ File::fromStdout
 */
File File::fromStdout()
{
	File out = {};
	out.handle = (void*)1;
	out.path = "stdout"_str;
	out.setWritable();
	out.setOpen();
	return out;

}

}
