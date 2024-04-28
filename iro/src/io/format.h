/*
 *  Formatting utils
 */

#ifndef _ilo_io_format_h
#define _ilo_io_format_h

#include "../common.h"
#include "../unicode.h"

namespace iro::io
{

struct IO;

/* ------------------------------------------------------------------------------------------------ Formattable
 */
template<typename T>
concept Formattable = requires(IO* io, T x)
{
	format(io, x);
};

/* ------------------------------------------------------------------------------------------------ Primitive formatting
 */
s64 format(IO* io, u8  x);
s64 format(IO* io, u16 x);
s64 format(IO* io, u32 x);
s64 format(IO* io, u64 x);
s64 format(IO* io, s8  x);
s64 format(IO* io, s16 x);
s64 format(IO* io, s32 x);
s64 format(IO* io, s64 x);
s64 format(IO* io, f32 x);
s64 format(IO* io, f64 x);
s64 format(IO* io, b8  x);
s64 format(IO* io, str x);
s64 format(IO* io, char x);
s64 format(IO* io, void* x);
s64 format(IO* io, const char* x);

/* ------------------------------------------------------------------------------------------------ Generic variadic formatting
 */
template<Formattable... T>
s64 formatv(IO* io, T... args)
{
	s64 accumulator = 0;
	((accumulator += format(io, args)), ...);
	return accumulator;
}

/* ================================================================================================ SanitizeControlCharacters
 *  
 */ 
struct SanitizeControlCharacters
{
	const str& x;
	SanitizeControlCharacters(const str& in) : x(in) {};
};

s64 format(IO* io, const SanitizeControlCharacters& x);

}

#endif // _ilo_io_format_h
