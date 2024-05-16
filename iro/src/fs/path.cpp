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
	
	if (path.len == 0)
		return path;

	if (path.len == 1)
	{
		if (path.bytes[0] == '/')
			return path;
		return ""_str;
	}

	if (path.last() == '/')
		path.len -= 1;

	auto pos = path.findLast('/');
	if (!pos)
		return path;
	return path.sub(pos+1);
}

/* ------------------------------------------------------------------------------------------------ Path::removeBasename
 */
void Path::removeBasename()
{
	if (buffer.len == 0)
		return;

	if (buffer.len == 1)
	{
		buffer.clear(); // ??
	}

	auto path = buffer.asStr();

	if (path.last() == '/')
		path.len -= 1;

	auto pos = path.findLast('/');
	if (!pos.found())
	{
		buffer.clear();
	}
	else
	{
		buffer.len = pos+1;
		buffer.rewind();
	}
}

/* ------------------------------------------------------------------------------------------------ Path::exists
 */
b8 Path::exists(str path)
{
	return platform::fileExists(path);
}

/* ------------------------------------------------------------------------------------------------ Path::isRegularFile
 */
b8 Path::isRegularFile(str path)
{
	// this is kind of an inefficient way to check this but oh well
	return FileInfo::of(path).kind == FileKind::Regular;
}

/* ------------------------------------------------------------------------------------------------ Path::isDirectory
 */
b8 Path::isDirectory(str path)
{
	// this is kind of an inefficient way to check this but oh well
	return FileInfo::of(path).kind == FileKind::Directory;
}

/* ------------------------------------------------------------------------------------------------ Path::matches
 *  TODO(sushi) implement alternative braces 
 *              (eg. path.{cpp,h} matches both 'path.cpp' and 'path.h')
 *              and implement character classes
 *              if i ever need them or if someone else wants them
 *
 *  NOTE this ignores dotfiles! I'll implement that being optional eventually.
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
	
	s64 name_backup = -1;
	s64 pattern_backup = -1;

	b8 nodot = true;
	
	while (name_pos < name_len)
	{
		if (pattern_pos < pattern_len)
		{
			switch (pattern.bytes[pattern_pos])
			{
			case '*':
				if (nodot && name.bytes[name_pos] == '.')
					break;
				name_backup = name_pos;
				pattern_pos += 1;
				pattern_backup = pattern_pos;
				continue;

			case '?':
				if (nodot && name.bytes[name_pos] == '.')
					break;
				if (name.bytes[name_pos] == '/')
					break;
				name_pos += 1;
				pattern_pos += 1;
				continue;

			case '\\':
				if (pattern_pos + 1 < pattern_len)
					pattern_pos += 1;

			default: 
				{
					if (pattern.bytes[pattern_pos] == '/' && name.bytes[name_pos] != '/')
						break;

					nodot = pattern.bytes[pattern_pos] == '/';

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

		if (pattern_backup == -1 || name_backup != -1 && name.bytes[name_backup] == '/')
			return false;
		// star loop: backtrack to the last * but dont jump over /
		name_backup += 1;
		name_pos = name_backup;
		pattern_pos = pattern_backup;
	}

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

// old Path::matches that did too much IDK maybe use for something else later?? i just dont want to 
// get rid of this
#if 0
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

	b8 nodot = true;
	while (name_pos < name_len)
	{
		if (pattern_pos < pattern_len)
		{
			switch (pattern.bytes[pattern_pos])
			{
			case '*':
				if (nodot && name.bytes[name_pos] == '.')
					break;
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
				// match anything except '.' after /
				if (nodot && name.bytes[name_pos] == '.')
					break;
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
	
					nodot = pattern.bytes[pattern_pos] == '/';

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
#endif
