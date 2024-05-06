/*
 *  Path util
 */

#ifndef _iro_path_h
#define _iro_path_h

#include "common.h"
#include "unicode.h"
#include "io/io.h"

namespace iro::fs
{

struct Path
{
	io::Memory buffer; // dynamic buffer for now, maybe make static later
	
	static Path create(str s = {});

	b8   init();
	void destroy();

	Path copy();

	// Returns the final component of this path eg.
	// src/fs/path.h -> path.h
	str basename();


};



}

#endif // _iro_path_h
