/*
 *  Primitive typedefs and defines that are used throughout all projects.
 *  This is literally included everywhere so try not to bloat it!
 */

#ifndef _iro_common_h
#define _iro_common_h

#include "stdint.h"
#include "stddef.h"

/* ----------------------------------------------
 *  Nice typedefs
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

static const u8  MAX_U8  = 0xff;
static const u16 MAX_U16 = 0xffff;
static const u32 MAX_U32 = 0xffffffff;
static const u64 MAX_U64 = 0xffffffffffffff;

static const s8  MAX_S8  = 127;
static const s8  MIN_S8  = -MAX_S8 - 1;
static const s16 MAX_S16 = 32767;
static const s16 MIN_S16 = -MAX_S16 - 1;
static const s32 MAX_S32 = 2147483647;
static const s32 MIN_S32 = -MAX_S32 - 1;
static const s64 MAX_S64 = 9223372036854775807;
static const s64 MIN_S64 = -MAX_S64 - 1;

static const f32 MAX_F32 = 3.402823466e+38f;
static const f32 MIN_F32 = -MAX_F32;
static const f64 MAX_F64 = 1.79769313486231e+308;
static const f64 MIN_F64 = -MAX_F64;

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

template<typename V, typename... T>
inline b8 matchSeq(V* v, T... args)
{
  s32 accum = -1;
  return ((accum += 1, v[accum] == args) && ...);
}

template<typename V, typename... T>
inline b8 matchSeqRev(V* v, T... args)
{
  s32 accum = sizeof...(T) + 1;
  return ((accum -= 1, v[-accum] == args) && ...);
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

template <class F> 
struct deferrer 
{ 
  F f; 
  ~deferrer() { f(); } 
};
struct defer_dummy {};

template <class F> 
deferrer<F> operator*(defer_dummy, F f) { return {f}; }

template<typename F> 
struct deferrer_with_cancel 
{ 
  b8 canceled; 
  F f; 
  ~deferrer_with_cancel() { if (!canceled) f(); }  
  void cancel() { canceled = true; } 
};
struct defer_with_cancel_dummy {};

template <class F> 
deferrer_with_cancel<F> operator*(defer_with_cancel_dummy, F f) { return {false, f}; }

#  define DEFER_(LINE) zz_defer##LINE
#  define DEFER(LINE) DEFER_(LINE)
#  define defer auto DEFER(__LINE__) = defer_dummy{} *[&]()
#  define deferWithCancel defer_with_cancel_dummy{} *[&]()
#endif //#ifndef defer

#define STRINGIZE_(a) #a
#define STRINGIZE(a) STRINGIZE_(a)
#define GLUE_(a,b) a##b
#define GLUE(a,b) GLUE_(a,b)

template<typename T>
inline T max(T a, T b) { return a > b? a : b; }

template<typename T>
inline T min(T a, T b) { return a > b? b : a; }

#endif // _iro_common_h
