/*
 *  Formatting utils.
 *
 *  This file is for defining the 'format' interface. Format functions for types other than
 *  the formatting wrappers and primitive types should go where those types are defined.
 *
 *  Eg. the format function for JSON is next to its definition, not here.
 */

#ifndef _ilo_io_format_h
#define _ilo_io_format_h

#include "../common.h"
#include "../unicode.h"
#include "concepts"

namespace iro::io
{

struct IO;

/* ----------------------------------------------------------------------------
 *  Defines the interface for 'format' functions, which take an IO* to write to 
 *  and a variable of some type and is expected to write some formatted output 
 *  to the given IO* then return the number of bytes written.
 */
template<typename T>
concept Formattable = requires(IO* io, T x)
{
  { format(io, x) } -> std::same_as<s64>;
};

/* ----------------------------------------------------------------------------
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
s64 format(IO* io, String x);
s64 format(IO* io, char x);
s64 format(IO* io, void* x);
s64 format(IO* io, const void* x);
s64 format(IO* io, const char* x);

/* ----------------------------------------------------------------------------
 */
s64 formatv(IO* io, Formattable auto&&... args)
{
  s64 accumulator = 0;
  ((accumulator += format(io, args)), ...);
  return accumulator;
}

/* ----------------------------------------------------------------------------
 *  Types that wrap other formattable types and perform extra formatting on 
 *  top of them.
 *
 *  TODO(sushi) build up a larger library of formatters later
 */

/* ============================================================================
 */ 
template<typename T>
struct SanitizeControlCharacters
{
  const T& x;
  SanitizeControlCharacters<T>(const T& in) : x(in) {}
};

template<typename T> // deduction guide
SanitizeControlCharacters(T x) -> SanitizeControlCharacters<T>;

s64 format(IO* io, const SanitizeControlCharacters<char>& x);
s64 format(IO* io, const SanitizeControlCharacters<String>& x);

/* ============================================================================
 */ 
template<typename T>
struct Hex
{
  T& x;
  Hex(T& x) : x(x) {}
};

s64 format(IO* io, Hex<u8>  x);
s64 format(IO* io, Hex<u16> x);
s64 format(IO* io, Hex<u32> x);
s64 format(IO* io, Hex<u64> x);
s64 format(IO* io, Hex<s8>  x);
s64 format(IO* io, Hex<s16> x);
s64 format(IO* io, Hex<s32> x);
s64 format(IO* io, Hex<s64> x);

}

#endif // _ilo_io_format_h
