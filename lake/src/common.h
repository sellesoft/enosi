#ifndef _lpp_common_h
#define _lpp_common_h

#include "stdint.h"

/* ----------------------------------------------
 *	TODO(sushi) add more log levels and make them
 *	            dependent on defines.
 */
#define TRACE(fmt, ...)                        \
	do {                                       \
		fprintf(stdout, "\e[36mtrace\e[0m: "); \
		fprintf(stdout, fmt, ##__VA_ARGS__);   \
	} while(0); 

/* ----------------------------------------------
 *	Nice typedefs
 */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef float    f32;
typedef double   f64;
typedef u8       b8; // booean type

/* ----------------------------------------------
 *	Counted string type
 */
struct str
{
	u8* s;
	s32 len;

	b8 isempty();
	u64 hash();

};

template<int N>
consteval u64 static_string_hash(const char s[N])
{
	u64 n = N;
	u64 seed = 14695981039;
	while (n--)
	{
		seed ^= (u8)*s;
		seed *= 1099511628211; //64bit FNV_prime
		s++;
	}
	return seed;
}

#define strl(x) { (u8*)u8##x, sizeof(u8##x)-1 }

/* ----------------------------------------------
 *	Memory wrappers in case I ever want to alter
 *  this behavior later or add memory tracking.
 */

struct Mem
{
	void* allocate(u64 n_bytes);
	void* reallocate(void* ptr, u64 n_bytes);
	void  free(void* ptr);
	void  copy(void* dst, void* src, u64 bytes);
};

extern Mem mem;

/* ----------------------------------------------
 *	Basic dynamic string type
 */
struct dstr
{
	u8* s;
	s32 len;
	s32 space;

	static dstr create(const char* s = 0);

	void destroy();

	void append(const char* s);
	void append(str s);

	template<typename T>
	void append(T x);
};

/* ----------------------------------------------
 *  Printing utils
 */

void print(const char* s);
void print(str s);
void print(u64 x);

template<typename... T>
void error(T...args)
{
	print("error: ");
	(print(args), ...);
	print("\n");
}

template<typename... T>
void error(str path, u64 line, u64 column, T... args)
{
	print(path);
	print(":");
	print(line);
	print(":");
	print(column);
	print(": ");
	error(args...);
}

#endif // _lpp_common_h
