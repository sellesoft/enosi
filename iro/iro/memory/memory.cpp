#include "memory.h"
#include "string.h"

namespace iro::mem
{

void copy(void* dst, void* src, u64 bytes)
{
	memcpy(dst, src, bytes);
}

void move(void* dst, void* src, u64 bytes)
{
	memmove(dst, src, bytes);
}

}
