#ifndef _lake_lake_h
#define _lake_lake_h

#include "common.h"

struct Lexer;
struct Parser;

struct Lake
{
	str path;

	u8* buffer;

	Parser* parser;

	void init(str path);
	void run();

private:

};

extern Lake lake;

#endif
