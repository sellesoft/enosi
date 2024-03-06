#include "common.h"

#include "stdio.h"
#include "stdlib.h"
#include "stdarg.h"
#include "ctype.h"

template<typename... T>
void fatal_error(const char* filename, s64 line, s64 column, T...args)
{
	fprintf(stdout, "%s", filename);
	fprintf(stdout, ":%li:%li: ", line, column);
	fprintf(stdout, "fatal_error");
	fprintf(stdout, ": ");
	exit(1);
}

int main() 
{
	
}
