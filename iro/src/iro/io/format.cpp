#include "io.h"
#include "format.h"

#include "../containers/stackarray.h"

#include "stdlib.h"
#include "stdio.h"

namespace iro::io
{

s64 format(IO* io, u8 x)
{
  u8  buffer[32];
  u64 len = snprintf((char*)buffer, 32, "%hhu", x);
  return io->write({buffer, len});
}

s64 format(IO* io, u16 x)
{
  u8  buffer[32];
  u64 len = snprintf((char*)buffer, 32, "%hu", x);
  return io->write({buffer, len});
}

s64 format(IO* io, u32 x)
{
  u8  buffer[32];
  u64 len = snprintf((char*)buffer, 32, "%u", x);
  return io->write({buffer, len});
}

s64 format(IO* io, u64 x)
{
  u8  buffer[32];
  u64 len = snprintf((char*)buffer, 32, "%llu", x);
  return io->write({buffer, len});
}

s64 format(IO* io, s8 x)
{
  u8  buffer[32];
  u64 len = snprintf((char*)buffer, 32, "%hhi", x);
  return io->write({buffer, len});
}

s64 format(IO* io, s16 x)
{
  u8  buffer[32];
  u64 len = snprintf((char*)buffer, 32, "%hi", x);
  return io->write({buffer, len});
}

s64 format(IO* io, s32 x)
{
  u8  buffer[32];
  u64 len = snprintf((char*)buffer, 32, "%i", x);
  return io->write({buffer, len});
}

s64 format(IO* io, s64 x)
{
  u8  buffer[32];
  u64 len = snprintf((char*)buffer, 32, "%lli", x);
  return io->write({buffer, len});
}

s64 format(IO* io, f32 x)
{
  u8  buffer[32];
  u64 len = snprintf((char*)buffer, 32, "%g", x);
  return io->write({buffer, len});
}

s64 format(IO* io, f64 x)
{
  u8  buffer[32];
  u64 len = snprintf((char*)buffer, 32, "%g", x);
  return io->write({buffer, len});
}

s64 format(IO* io, String x)
{
  return io->write(x);
}

s64 format(IO* io, char x)
{
  return io->write({(u8*)&x, 1});
}

s64 format(IO* io, void* x)
{
  u8 buffer[32];
  u64 len = snprintf((char*)buffer, 32, "%p", x);
  return io->write({buffer, len});
}

s64 format(IO* io, const void* x)
{
  u8 buffer[32];
  u64 len = snprintf((char*)buffer, 32, "%p", x);
  return io->write({buffer, len});
}

s64 format(IO* io, const char* x)
{
  Bytes slice = {(u8*)x, 0};
  while (x[slice.len])
    slice.len += 1;
  return io->write(slice);
}

static String getSanitizedCharacter(char c)
{
  switch (c)
  {
    case '\a': return "\\a"_str;
    case '\b': return "\\b"_str;
    case '\e': return "\\e"_str;
    case '\f': return "\\f"_str;
    case '\n': return "\\n"_str;
    case '\r': return "\\r"_str;
    case '\t': return "\\t"_str;
    case '\v': return "\\v"_str;
    case '\0': return "\\0"_str;
  }
  return nil;
}

s64 format(IO* io, const SanitizeControlCharacters<char>& x)
{
  String sanitized = getSanitizedCharacter(x.x);
  if (isnil(sanitized))
    return format(io, x.x);
  else
    return io->write(sanitized);
}

s64 format(IO* io, const SanitizeControlCharacters<String>& x)
{
  StackArray<u8, 255> buffer;
  
  auto flush = [&buffer, io]() { io->write(buffer.asSlice()); buffer.len = 0; };

  s64 bytes_written = 0;
  auto write = [&buffer, &bytes_written, flush](String s)
  {
    if (buffer.len + s.len > buffer.capacity())
      flush();

    for (u64 i = 0; i < s.len; ++i)
      *buffer.push() = s.ptr[i];
    
    bytes_written += s.len;
  };

  String s = x.x;
  while (!s.isEmpty())
  {
    utf8::Codepoint c = s.advance();
    if (c.advance == 1)
    {
      String sanitized = getSanitizedCharacter(c.codepoint);
      if (isnil(sanitized))
        write(String::from((u8*)&c.codepoint, c.advance));
      else
        write(sanitized);
    }
    else
      write(String::from((u8*)&c.codepoint, c.advance));
  }

  if (buffer.len)
    flush();

  return bytes_written;
}

s64 format(IO* io, Hex<u8> x)
{
  u8  buffer[32];
  u64 len = snprintf((char*)buffer, 32, "0x%hhx", x.x);
  return io->write({buffer, len});
}

s64 format(IO* io, Hex<u16> x)
{
  u8  buffer[32];
  u64 len = snprintf((char*)buffer, 32, "0x%hx", x.x);
  return io->write({buffer, len});
}

s64 format(IO* io, Hex<u32> x)
{
  u8  buffer[32];
  u64 len = snprintf((char*)buffer, 32, "0x%x", x.x);
  return io->write({buffer, len});
}

s64 format(IO* io, Hex<u64> x)
{
  u8  buffer[32];
  u64 len = snprintf((char*)buffer, 32, "0x%llx", x.x);
  return io->write({buffer, len});
}

s64 format(IO* io, Hex<s8> x)
{
  u8  buffer[32];
  u64 len = snprintf((char*)buffer, 32, "0x%hhx", x.x);
  return io->write({buffer, len});
}

s64 format(IO* io, Hex<s16> x)
{
  u8  buffer[32];
  u64 len = snprintf((char*)buffer, 32, "0x%hx", x.x);
  return io->write({buffer, len});
}

s64 format(IO* io, Hex<s32> x)
{
  u8  buffer[32];
  u64 len = snprintf((char*)buffer, 32, "0x%x", x.x);
  return io->write({buffer, len});
}

s64 format(IO* io, Hex<s64> x)
{
  u8  buffer[32];
  u64 len = snprintf((char*)buffer, 32, "%llx", x.x);
  return io->write({buffer, len});
}

}
