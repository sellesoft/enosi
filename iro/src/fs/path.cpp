#include "path.h"

#include "fs/fs.h"
#include "platform.h"

#include "logger.h"

namespace iro::fs
{

static Logger logger = Logger::create("iro.fs.path"_str, Logger::Verbosity::Trace);

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
 *  Thanks to Robert A. van Engelen for this implementation.
 *  https://www.codeproject.com/Articles/5163931/Fast-String-Matching-with-Wildcards-Globs-and-Giti
 */
b8 Path::matches(str name, str pattern)
{
	s64 name_pos = 0;
	s64 pattern_pos = 0;
	s64 name_len = name.len;
	s64 pattern_len = pattern.len;
	
	s64 name_backup1 = -1;
	s64 name_backup2 = -1;
	s64 pattern_backup1 = -1;
	s64 pattern_backup2 = -1;

	while (name_pos < name_len)
	{
		defer
		{
			str namec = {name.bytes + name_pos, name.len - name_pos};
			str pattc = {pattern.bytes + pattern_pos, pattern.len - pattern_pos};
			INFO("\n",
				"name: ", namec, "\n",
				"patt: ", pattc, "\n");
		};

		if (pattern_pos < pattern_len)
		{
			switch (pattern.bytes[pattern_pos])
			{
			case '*':
				pattern_pos += 1;
				if (pattern.bytes[pattern_pos] == '*')
				{
					pattern_pos += 1;
					if (pattern_pos > pattern_len)
						return true; // match anything after **
					if (pattern.bytes[pattern_pos] != '/')
						return false;
					// new **-loop, discard *-loop
					name_backup1 = 
					pattern_backup1 = -1;
					name_backup2 = name_pos;
					pattern_pos += 1;
					pattern_backup2 = pattern_pos;
					continue;
				}
				// trailing * matches everything except /
				name_backup1 = name_pos;
				pattern_backup1 = pattern_pos;
				continue;

			case '?':
				// match char except /
				if (name.bytes[name_pos] == '/')
					break;
				name_pos += 1;
				pattern_pos += 1;
				break;

			case '\\':
				if (pattern_pos + 1 < pattern_len)
					pattern_pos += 1;

			default:
				{
					if (pattern.bytes[pattern_pos] == '/' && name.bytes[name_pos] != '/')
						break;

					// decode at positions
					utf8::Codepoint pattern_codepoint = utf8::decodeCharacter(pattern.bytes + pattern_pos, pattern.len - pattern_pos);
					utf8::Codepoint name_codepoint = utf8::decodeCharacter(name.bytes + name_pos, name.len - name_pos);

					if (pattern_codepoint != name_codepoint)
						break;

					pattern_pos += pattern_codepoint.advance;
					name_pos += name_codepoint.advance;
					continue;
				}
			}
		}

		if (pattern_backup1 != -1 && name.bytes[name_pos] != '/')
		{
			name_backup1 += 1;
			name_pos = name_backup1;
			pattern_pos = pattern_backup1;
			continue;
		}

		if (pattern_backup2 != -1)
		{
			// **-loop, backtrack to last **
			name_backup2 += 1;
			name_pos = name_backup2;
			pattern_pos = pattern_backup2;
			continue;
		}

		return false;
	}

	// ignore trailing stars
	while (pattern_pos < pattern_len && pattern.bytes[pattern_pos] == '*')
		pattern_pos += 1;

	return pattern_pos >= pattern_len;
}

/* ------------------------------------------------------------------------------------------------ Path::matches
 */
b8 Path::matches(str pattern)
{
	return Path::matches(buffer.asStr(), pattern);
}

}
