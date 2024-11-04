--- 
--- FFI declarations for iro. This is loaded automatically by iro's LuaState.
---
--- TODO(sushi) this file should be automatically generated!
---             since this is compiled into the exe we can probably get away
---             with opening the files this stuff is contained in and 
---             forming the string dynamically. Unfortuntaely this is 
---             complicated by those files being written in C++ and this 
---             needing to be pure C.
---             We could maybe use lpp with clang to do it automatically
---             but that would make iro dependent on lpp, which isn't a good
---             idea until lpp is stable and can be portably shipped with the
---             repo.

local ffi = require "ffi"
ffi.cdef 
[[
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

  typedef struct String
  {
    const u8* s;
    u64 len;
  } String;

  typedef struct
  {
    const u8* ptr;
    u64 len;
  } Bytes;
]]
