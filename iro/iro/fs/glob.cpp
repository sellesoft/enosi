#include "glob.h"

namespace iro::fs
{

/* ------------------------------------------------------------------------------------------------
 */
Globber Globber::create(String pattern, mem::Allocator* allocator)
{
  Globber out = {};
  out.allocator = allocator;
  out.pattern = pattern.allocateCopy(allocator);
  out.part_pool = PartPool::create(allocator);
  out.part_list = PartList::create(allocator);
  out.compilePattern();
  return out;
}

/* ------------------------------------------------------------------------------------------------
 */
void Globber::destroy()
{
  allocator->free(pattern.ptr);
  part_pool.deinit();
  part_list.deinit();
}

/* ------------------------------------------------------------------------------------------------
 */
void Globber::compilePattern()
{
  using enum Part::Kind;

  u8* start = pattern.end();
  u8* end = start;
  b8 constant = true;
  b8 escaped = false;
  
  Part* last_part = nullptr;

  auto processPart = [&]()
  {

    if (start >= end)
      return;

    u64 len = end - start; 

    Part::Kind kind;
    if (!last_part)
    {
      if (constant)
      {
        kind = ConstantEntry;
      }
      else
      {
        kind = MatchEntry;
      }
    }
    else
    {
      if (constant)
      {
        if (last_part->kind == ConstantEntry ||
          last_part->kind == ConstantDirectory)
        {
          last_part->raw.len += last_part->raw.ptr - start;
          last_part->raw.ptr = start;
          return;
        }
        else
        {
          kind = ConstantDirectory;
        }
      } 
      else if (len == 2 && start[0] == '*' && start[1] == '*')
      {
        kind = DoubleStar;
      }
      else
      {
        kind = MatchDirectory;
      }
    }

    last_part = pushPart(kind, {start, len});
  };

  if (pattern.last() == '/')
  {
    last_part = pushPart(DirectoriesOnly, nil);
  }

  for (s32 i = pattern.len - 1; i >= 0; i--)
  {
    start -= 1;

    if (escaped)
    {
      escaped = false;
      continue;
    }

    switch (*start)
    {
    case '?': 
    case '*':
      if (i && start[-1] == '\\')
        escaped = true;
      else
        constant = false;
      break;

    case '/':
      start += 1;
      processPart();
      start -= 1;
      end = start;
      constant = true;
      break;
    }
  }

  processPart();
}

/* ------------------------------------------------------------------------------------------------
 */
Globber::Part* Globber::pushPart(Part::Kind kind, String raw)
{
  Part* p = part_pool.add();
  p->kind = kind;
  p->raw = raw;
  part_list.pushHead(p);
  return p;
}

}

extern "C"
{

using namespace iro;
using namespace iro::fs;

struct GlobResult
{
  String* paths;
  s32 path_count;
};

EXPORT_DYNAMIC
GlobResult iro_glob(String pattern)
{
  auto glob = Globber::create(pattern);
  auto matches = Array<String>::create();
  glob.run(
    [&matches](fs::Path& p)
    {
      String match = p.buffer.asStr();
      *matches.push() = match.allocateCopy();
      return true;
    });
  glob.destroy();

  return {matches.arr, matches.len()};
}

EXPORT_DYNAMIC
void iro_globFree(GlobResult result)
{
  auto arr = Array<String>::fromOpaquePointer(result.paths);
  for (String& s : arr)
    mem::stl_allocator.free(s.ptr);

  arr.destroy();
}

}
