/*
 *  Primitive typedefs and defines that are used throughout all projects.
 *  This is literally included everywhere so try not to bloat it!
 */

#ifndef _iro_common_h
#define _iro_common_h

#include "stdint.h"
#include "stddef.h"

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

// TODO(sushi) move these string things elsewhere
consteval s64 constevalStrlen(const char* s) {
    s64 i = 0;
    while(s[i])
        i++;
    return i;
}

// hashes a null terminated string
// TODO(sushi) really gotta move this hashing stuff elsewhere
static u64 cStrHash(const char* s)
{
	u64 seed = 14695981039;
	while (*s)
	{
		seed ^= *s;
		seed *= 1099511628211;
		s += 1;
	}
	return seed;
}

consteval u64 staticStringHash(const char* s, size_t len)
{
	u64 seed = 14695981039;
	for (s32 i = 0; i < len; i++)
	{
		seed ^= (u8)s[i];
		seed *= 1099511628211;
	}
	return seed;
}

consteval u64 operator ""_hashed (const char* s, size_t len)
{
	return staticStringHash(s, len);
}

// Nice sugar for matching a single value against any of 'args'.
template<typename V, typename... T>
inline b8 matchAny(V v, T... args)
{
	return ((v == args) || ...);
}

// Useful to be able to call normally-inlined functions from a debugger
#if IRO_DEBUG
#define release_inline 
#else
#define release_inline inline
#endif

// TODO(sushi) eventually this would be better placed in platform.h, but I don't really like the 
//             idea of having to include all of platform.h into anything that uses this,
//             so if I eventually reorganize platform to be more modular, move this there
//         and well, ideally this could be replaced by something in lpp anyways.
#ifdef IRO_LINUX
// NOTE(sushi) in order for this to work properly with executables (my use case of it) the obj
//             files need to be compiled with the linker flag -fvisiblity=hidden and the executable
//             then must be linked with the flag -E (aka --export-dynamic).
//             Windows actually does better here.. __declspec(dllexport) works regardless of if 
//             a shared lib or executable is being linked.
#define EXPORT_DYNAMIC __attribute__((visibility("default")))
#else
#define EXPORT_DYNAMIC "EXPORT_DYNAMIC is not setup for this platform!"
#endif

#ifndef defer
struct defer_dummy {};
struct defer_with_cancel_dummy {};
template <class F> struct deferrer { F f; ~deferrer() { f(); } };
template<typename F> struct deferrer_with_cancel { b8 canceled; F f; ~deferrer_with_cancel() { if (!canceled) f(); }  void cancel() { canceled = true; } };
template <class F> deferrer<F> operator*(defer_dummy, F f) { return {f}; }
template <class F> deferrer_with_cancel<F> operator*(defer_with_cancel_dummy, F f) { return {false, f}; }
#  define DEFER_(LINE) zz_defer##LINE
#  define DEFER(LINE) DEFER_(LINE)
#  define defer auto DEFER(__LINE__) = defer_dummy{} *[&]()
#  define deferWithCancel defer_with_cancel_dummy{} *[&]()
#endif //#ifndef defer

#define STRINGIZE_(a) #a
#define STRINGIZE(a) STRINGIZE_(a)
#define GLUE_(a,b) a##b
#define GLUE(a,b) GLUE_(a,b)

#endif // _iro_common_h
