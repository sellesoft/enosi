/*
 *  General helper for mapping byte offsets in a document to 
 *  a line and column.
 */

#ifndef _iro_linemap_h
#define _iro_linemap_h

#include "common.h"
#include "unicode.h"

#include "containers/array.h"

using namespace iro;

namespace iro
{

// the index of the line and its byte offset into the buffer
struct LineOffset
{
	u64 line, offset;
};

/* ================================================================================================ LineMap
 */
struct LineMap
{
	Array<u64> line_offsets;
	b8 cached;


	/* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
	 */


	b8   init(mem::Allocator* allocator = &mem::stl_allocator);
	void deinit();

	b8 cache(Bytes bytes);

	void touchedBuffer() { cached = false; }

	LineOffset getLine(u64 offset);
};

}

DefineNilValue(LineMap, {nil}, { return isnil(x.line_offsets); });
DefineMove(LineMap, { to.cached = from.cached; move(from.line_offsets, to.line_offsets); });

#endif // _iro_linemap_h
