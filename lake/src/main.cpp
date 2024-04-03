#include "lake.h"
#include "stdlib.h"

int main(const int argc, const char* argv[]) 
{
	lake.init(strl("lakefile"), argc, argv);
	lake.run();
}
