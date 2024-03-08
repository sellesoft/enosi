#include "common.h"

#include "stdlib.h"
#include "ctype.h"
#include "string.h"
#include "stdio.h"

b8 str::isempty()
{
	s32 x = len;
	while (x--) if (!isspace(s[x])) return 0;
	return 1;
}

u64 str::hash()
{
	u8* p = s;
	s32 x = len;
	u64 seed = 14695981039;
	while (x--)
	{
		seed ^= (u8)*p;
		seed *= 1099511628211;
		p += 1;
	}
	return seed;
}

Mem mem;

void* Mem::allocate(u64 n_bytes)
{
	return malloc(n_bytes);
}

void Mem::free(void* ptr)
{
	::free(ptr);
}

void* Mem::reallocate(void* ptr, u64 n_bytes)
{
	return realloc(ptr, n_bytes);
}

void Mem::copy(void* dst, void* src, u64 n_bytes)
{
	memcpy(dst,src,n_bytes);
}

void Mem::move(void* dst, void* src, u64 n_bytes)
{
	memmove(dst, src, n_bytes);
}

void print(const char* s) { printf("%s", s); }

void print(str s) { printf("%.*s", s.len, s.s); }
void print(u32 x) { printf("%u", x); }
void print(u64 x) { printf("%lu", x); }
void print(s32 x) { printf("%i", x); }
void print(s64 x) { printf("%li", x); }

void print(char x) { printf("%c", x); }


