#include "fs.h"
#include "platform.h"
#include "io/format.h"
#include "logger.h"

namespace iro::fs
{

Logger logger = Logger::create("iro.fs"_str, Logger::Verbosity::Warn);


/* ------------------------------------------------------------------------------------------------ File::from
 */
File File::from(str path, OpenFlags flags, mem::Allocator* allocator)
{
	return from(move(Path::from(path, allocator)), flags);
}

File File::from(Path path, OpenFlags flags, mem::Allocator* allocator)
{
	return from(move(Path::from(path.buffer.asStr(), allocator)), flags);
}

File File::from(Moved<Path> path, OpenFlags flags)
{
	if (path == nil)
	{
		ERROR("nil path given to File::from()\n");
		return nil;
	}

	File out = nil;

	if (!out.open(path, flags))
		return nil;

	return out;
}


/* ------------------------------------------------------------------------------------------------ File::open
 */
b8 File::open(Moved<Path> path, OpenFlags flags)
{
	if (isOpen())
	{
		path.destroy();
		return false; 
	}

	if (!platform::open(&handle, path.buffer.asStr(), flags))
	{
		path.destroy();
		return false;
	}

	setOpen();

	if (flags.test(OpenFlag::Read))
		setReadable();

	if (flags.test(OpenFlag::Write))
		setWritable();

	this->path = path;

	return true;
}

/* ------------------------------------------------------------------------------------------------ File::close
 */
void File::close()
{
	if (!isOpen())
		return;

	platform::close(handle);
	
	path.destroy();

	unsetOpen();
	unsetReadable();
	unsetWritable();

	*this = nil;
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

/* ------------------------------------------------------------------------------------------------ File::fromFileDescriptor
 */
File File::fromFileDescriptor(u64 fd, OpenFlags flags)
{
	File out = {};
	out.handle = (void*)fd;

	out.setOpen();

	if (flags.test(OpenFlag::Read))
		out.setReadable();

	if (flags.test(OpenFlag::Write))
		out.setWritable();

	out.path = nil;

	return out;
}

File File::fromFileDescriptor(u64 fd, str name, OpenFlags flags, mem::Allocator* allocator)
{
	return fromFileDescriptor(fd, move(Path::from(name, allocator)), flags);
}

File File::fromFileDescriptor(u64 fd, Moved<Path> name, OpenFlags flags)
{
	File out = fromFileDescriptor(fd, flags);
	out.path = name;
	return out;
}

/* ------------------------------------------------------------------------------------------------ FileInfo::of
 */
FileInfo FileInfo::of(str path)
{
	assert(notnil(path) && path.len);

	FileInfo out = {};
	if (!platform::stat(&out, path))
		return FileInfo::invalid();
	return out;
}

FileInfo FileInfo::of(File file)
{
	return FileInfo::of(file.path.buffer.asStr());
}

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

namespace __internal
{

/* ------------------------------------------------------------------------------------------------ expand_brace_pattern
 */
b8 expand_brace_pattern(mem::Allocator* allocator, Array<io::Memory>* expansions, str pattern)
{
	u8* lbrace = nullptr;
	u8* rbrace = nullptr;
	u8* alt_start = nullptr;

	auto alternatives = Array<str>::create(16, allocator);

	s32 nesting = 0;
	b8 escaped = false;

	str pattern_save = pattern;

	while (!pattern.isEmpty())
	{
		if (escaped)
		{
			escaped = false;
			continue;
		}

		auto dc = utf8::decodeCharacter(pattern.bytes, pattern.len);
		if (!dc.isValid())
			return false;

		switch (dc.codepoint)
		{
		case '{':
			lbrace = pattern.bytes;
			nesting += 1;
			break;

		case '}':
			nesting -= 1;
			if (nesting == 0)
			{
				rbrace = pattern.bytes;
				u8* start = 1 + (alt_start? alt_start : lbrace);
				alternatives.push({start, u64(pattern.bytes - start)});
				goto done;
			}
			break;

		case ',':
			if (nesting == 1)
			{
				u8* start = 1 + (alt_start? alt_start : lbrace);
				alternatives.push({start, u64(pattern.bytes - start)});
				alt_start = pattern.bytes;
			}
			break;

		case '\\':
			escaped = true;
			break;
		}

		pattern.increment(dc.advance);
	}
done:;
	 
	if (lbrace && rbrace)
	{
		str front = {pattern_save.bytes, u64(lbrace - pattern_save.bytes)};
		str back  = {rbrace + 1, u64((pattern_save.bytes + pattern_save.len) - rbrace)};

		for (str alt : alternatives)
		{
			io::Memory brace_pattern;
			brace_pattern.open(front.len + back.len + alt.len);
			io::formatv(&brace_pattern, front, alt, back);
			if (!expand_brace_pattern(allocator, expansions, brace_pattern.asStr()))
				return false;
		}

	}
	else
	{
		auto exp = expansions->push();
		exp->open(pattern.len, allocator);
		exp->write(pattern);
	}

	return true;
}

}

}
