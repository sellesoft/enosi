#include "linemap.h"

namespace iro
{

/* ------------------------------------------------------------------------------------------------ LineMap::init
 */
b8 LineMap::init(mem::Allocator* allocator)
{
	line_offsets = Array<u64>::create(allocator);
	return true;
}

/* ------------------------------------------------------------------------------------------------ LineMap::deinit
 */
void LineMap::deinit()
{
	line_offsets.destroy();
}

/* ------------------------------------------------------------------------------------------------ LineMap::cache
 */
b8 LineMap::cache(Bytes bytes)
{
	if (cached)
		return true;

	line_offsets.clear();

	line_offsets.push(0);

	for (s64 i = 0; i < bytes.len; i++)
	{
		if (bytes[i] == '\n')
			line_offsets.push(i + 1);
	}

	cached = true;

	return true;
}

/* ------------------------------------------------------------------------------------------------ LineMap::getLine
 */
LineOffset LineMap::getLine(u64 offset)
{
	assert(cached);

	u64 l = 0,
		m = 0,
		r = line_offsets.len() - 1;

	u64 cur;
	while (l <= r)
	{
		m = l + (r - l) / 2;
		if (!m)
			break;
		cur = line_offsets[m];
		if (cur < offset)
			l = m + 1;
		else if (cur > offset)
			r = m - 1;
		else
			break;
	}

	if (cur > offset)
		cur = line_offsets[m - 1];

	return {m, cur};
}

}
