#ifndef _lake_lake_h
#define _lake_lake_h

#include "common.h"
#include "lexer.h"

struct Lake
{
	str path;

	u8* buffer;

	Lexer* lex;

	void init(str path);

	void run();

private:

};

extern Lake lake;

#endif
