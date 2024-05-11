#include "path.h"

#include "fs/fs.h"
#include "platform.h"

#include "logger.h"

namespace iro::fs
{

static Logger logger = Logger::create("iro.fs.path"_str, Logger::Verbosity::Warn);

/* ------------------------------------------------------------------------------------------------ Path::from
 */
Path Path::from(str s, mem::Allocator* allocator)
{
	Path out = {};
	out.init(s);
	return out;
}

/* ------------------------------------------------------------------------------------------------ Path::init
 */
b8 Path::init(str s, mem::Allocator* allocator)
{
	if (!buffer.open(s.len, allocator))
		return false;
	if (buffer.write(s) != s.len)
		return false;
	return true;
}

/* ------------------------------------------------------------------------------------------------ Path::destroy
 */
void Path::destroy()
{
	buffer.close();
	*this = nil;
}

/* ------------------------------------------------------------------------------------------------ Path::copy
 */
Path Path::copy()
{
	Path out = {};
	out.init(buffer.asStr());
	return out;
}

/* ------------------------------------------------------------------------------------------------ Path::clear
 */
void Path::clear()
{
	buffer.clear();
}

/* ------------------------------------------------------------------------------------------------ Path::basename
 */
str Path::basename()
{
	str path = buffer.asStr();
	auto pos = path.findLast('/');
	if (!pos)
		return path;
	return path.sub(pos+1);
}

/* ------------------------------------------------------------------------------------------------ Path::exists
 */
b8 Path::exists()
{
	return platform::file_exists(buffer.asStr());
}

/* ------------------------------------------------------------------------------------------------ Path::isRegularFile
 */
b8 Path::isRegularFile()
{
	if (!exists())
		return false;
	// this is kind of an inefficient way to check this but oh well
	return FileInfo::of(buffer.asStr()).kind == FileKind::Regular;
}

/* ------------------------------------------------------------------------------------------------ Path::isDirectory
 */
b8 Path::isDirectory()
{
	if (!exists())
		return false;
	// this is kind of an inefficient way to check this but oh well
	return FileInfo::of(buffer.asStr()).kind == FileKind::Directory;
}

/* ------------------------------------------------------------------------------------------------ Path::matches
 *  TODO(sushi) implement alternative braces 
 *              (eg. path.{cpp,h} matches both 'path.cpp' and 'path.h')
 *              and implement character classes
 *              if i ever need them or if someone else wants them
 *
 */
b8 Path::matches(str name, str pattern)
{
	// helper for reading the pattern and name
	struct Reader
	{
		str s;
		utf8::Codepoint current;

		Reader(str s) : s(s) { current = utf8::decodeCharacter(s.bytes, s.len); }

		b8 hasNext() { return s.len; }

		void next()
		{
			s.increment(current.advance);
			if (hasNext())
				current = utf8::decodeCharacter(s.bytes, s.len);
		}

		u8 peekByte()
		{
			if (hasNext())
				return *(s.bytes + current.advance);
			return 0;
		}

		void set(str n)
		{
			s = n;
			current = utf8::decodeCharacter(s.bytes, s.len);
		}
	};

	auto nread = Reader(name);
	auto pread = Reader(pattern);
	
	str next_p = nil;
	str next_n = nil;

	b8 escaped = false;

	for (;;)
	{
		if (escaped)
		{
			escaped = false;
			continue;
		}

		if (!nread.hasNext() && !pread.hasNext())
			return true;

		if (pread.hasNext())
		{
			switch (pread.current.codepoint)
			{
			default:
				if (nread.current == pread.current)
				{
					pread.next();
					nread.next();
					continue;
				}
				break;
			
			case '?':
				if (nread.hasNext())
				{
					nread.next();
					pread.next();
					continue;
				}
				break;

			case '*':
				{
					b8 dbl = '*' == pread.peekByte();

					if (!dbl && nread.current == '/')
					{
						pread.next();
						next_n = str::nil();
						continue;
					}
					else
					{
						next_p = pread.s;
						next_n = nread.s;
						next_n.increment(nread.current.advance);
						pread.next();
						if (dbl)
							pread.next();
						continue;
					}
				}
				break;
			}

			// no match was made, try again if we have a place to 
			// return to from a *
			if (nread.hasNext() && notnil(next_n))
			{
				nread.set(next_n);
				pread.set(next_p);
				continue;
			}
		}
		return false;
	}

}

/* ------------------------------------------------------------------------------------------------ Path::matches
 */
b8 Path::matches(str pattern)
{
	return Path::matches(buffer.asStr(), pattern);
}

}
