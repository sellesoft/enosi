#include "lake.h"
#include "platform.h"

Lake lake;

void Lake::init(str p)
{
	path = p;
	lex = (Lexer*)mem.allocate(sizeof(Lexer));
	buffer = platform::read_file(path);
	lex->init(buffer);
}

void Lake::run()
{
	
}
