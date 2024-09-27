/*
 *  Path globber.
 */

#ifndef _iro_glob_h
#define _iro_glob_h

#include "../common.h"
#include "../unicode.h"

#include "../containers/pool.h"
#include "../containers/list.h"
#include "../containers/array.h"
#include "../containers/stackarray.h"
#include "../containers/linked_pool.h"

#include "../io/io.h"
#include "../fs/path.h"
#include "../fs/fs.h"

#include "../memory/bump.h"

#include "../logger.h"

#include "concepts"

namespace iro::fs
{

template<typename F>
concept GlobberCallback = requires(F f, Path& p)
{
  { f(p) } -> std::convertible_to<b8>;
};

/* ================================================================================================ Globber
 */
struct Globber
{
    /* ============================================================================================ Globber::Part
     */
  struct Part
  {
    enum class Kind
    {
      // A path segment that has no matching syntax in it
      // eg. /this/ and not /th?a*t/
      ConstantEntry,
      ConstantDirectory,
      // A path segment that is to be matched against a 
      // filesystem entry.
      MatchEntry,
      MatchDirectory,
      // A double star ** part that was followed by a '/'.
      DoubleStar,
      DirectoriesOnly,
    };

    Kind kind;

    str raw;
  };

  typedef Pool<Part> PartPool;
  typedef DList<Part> PartList;

  PartPool part_pool;
  PartList part_list;

  // NOTE(sushi) this is OWNED by this guy!!
  str pattern;

  mem::Allocator* allocator;


  /* -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
   */


  static Globber create(str pattern, mem::Allocator* allocator = &mem::stl_allocator);
  
  void destroy();

  void run(GlobberCallback auto f);

private:

  // internal stuff
  
  void compilePattern();

  Part* pushPart(Part::Kind kind, str raw);
  
};

/* ------------------------------------------------------------------------------------------------ Globber::run
 *  Based on Crystal's globbing stuff:
 *    https://github.com/crystal-lang/crystal/blob/4cea10199/src/dir/glob.cr#L48
 *  Until I get a better idea of how this should be implemented or forever if it works well enough.
 *
 *  TODO(sushi) eventually try and move as much of this logic into the cpp as possible. This is 
 *              only in the header atm because it uses a templated callback and so this func 
 *              should be reduced to only have to deal with that aspect of it (if possible).
 *
 *  NOTE(sushi) this uses quite a bit of memory cause im just tossing everything into a 
 *              bump allocator. maaaybe clean it up later, but im not too worried about it 
 *              because zsh uses more memory with the same globs so whatever.
 */
void Globber::run(GlobberCallback auto callback)
{
  using enum Part::Kind;

  Logger logger = Logger::create("globber.run"_str, Logger::Verbosity::Warn);

  PartList::Node* curn = part_list.head;
  if (!curn)
    return;

  // Just tossing all this memory into a bump alloc for now. Handle all this
  // memory stuff more gracefully later.
  mem::Bump bump;
  if (!bump.init())
    return;
  defer { bump.deinit(); };

  struct Entry
  {
    PartList::Node* node;
    Path path;
  };

  DLinkedPool<Entry> entry_stack = DLinkedPool<Entry>::create(&bump);

  entry_stack.pushTail({curn, nil});

  while (!entry_stack.isEmpty())
  {
    Entry entry = entry_stack.popTail();

    PartList::Node* node = entry.node;
    Part* part = node->data;
    Path path = entry.path;

    INFO("processing entry ", path, "\n");
    SCOPED_INDENT;

    switch (part->kind)
    {
    case ConstantEntry:
      {
        Path full = notnil(path)? path : Path::from(""_str, &bump);
        if (full.buffer.asStr().isEmpty())
          full.append(part->raw);
        else
          full.appendDir(part->raw);

        if (full.exists())
        {
          if (node->next)
          {
            // push to path stack and move to next 
            entry_stack.pushTail({node->next, full});
          }
          else
          {
            // if this is the last part then report the match
            if (!callback(full)) 
              goto done;
          }
        }
      }
      break;

    case ConstantDirectory:
      {
        INFO("ConstantDirectory: opening ", path, "\n");
        SCOPED_INDENT;

        Path full = notnil(path)? path.copy() : Path::from(""_str, &bump);
        full.makeDir().append(part->raw);
        
        INFO("full path is ", full, "\n");

        if (full.isDirectory())
        {
          INFO("ConstantDirectory: pushing dir ", full, "\n");
          entry_stack.pushTail({node->next, full});
        }
      }
      break;

    case DirectoriesOnly:
      {
        if (path.isDirectory())
        {
          if (!callback(path))
            goto done;
        }
      }
      break;

    case MatchEntry:
      {
        // ** handles matching entries, so we dont need to do it again
        if (node->prev && node->prev->data->kind == DoubleStar)
          break;

        INFO("MatchEntry: opening ", path, "\n");
        SCOPED_INDENT;

        StackArray<u8, 255> buffer;
        Dir dir = Dir::open(path);
        if (isnil(dir))
          goto err;
        defer { dir.close(); };
        
        Path full = path.copy().makeDir();
        auto rollback = full.makeRollback();

        for (;;)
        {
          buffer.len = dir.next({buffer.arr, buffer.capacity()});
          if (buffer.len == -1)
            goto err;
          if (buffer.len == 0)
            break;

          str entstr = str::from(buffer.asSlice());

          if (entstr == "."_str || entstr == ".."_str)
            continue;

          if (Path::matches(entstr, part->raw))
          {
            full.append(entstr);
            INFO("path ", full, " matches ", part->raw, "\n");

            if (!node->next)
            {
              if (!callback(full))
                goto done;
            }
            else if (full.isDirectory())
            {
              INFO("match is a directory\n");
              entry_stack.pushTail({node->next, full.copy()});
            }

            full.commitRollback(rollback);
          }
        }
      }
      break;

    case MatchDirectory:
      {

        INFO("MatchDirectory: opening ", path, "\n");
        SCOPED_INDENT;

        StackArray<u8, 255> buffer;
        Dir dir = Dir::open(path);
        if (isnil(dir))
          goto err;
        defer { dir.close(); };

        Path full = path.copy().makeDir();
        auto rollback = full.makeRollback();

        for (;;)
        {
          buffer.len = dir.next({buffer.arr, buffer.capacity()});

          if (buffer.len == -1)
            goto err;
          if (buffer.len == 0)
            break;

          str entstr = str::from(buffer.asSlice());

          if (entstr == "."_str || entstr == ".."_str)
            continue;

          if (Path::matches(entstr, part->raw))
          {
            full.append(entstr);
            INFO("path ", full, " matches ", part->raw, "\n");
            if (full.isDirectory())
            {
              INFO("path exists and is a directory\n");
              entry_stack.pushTail({node->next, full.copy()});
            }
          }

          full.commitRollback(rollback);
        }

      }
      break;

    case DoubleStar:
      {
        // recursively iterate over all files from the current path
        // and attempt to match the next part against them

        DLinkedPool<Dir> dir_stack = DLinkedPool<Dir>::create(&bump);
        DLinkedPool<Path> dir_path_stack = DLinkedPool<Path>::create(&bump);

        dir_path_stack.pushTail(path);


        INFO("DoubleStar: opening ", path, "\n");
        SCOPED_INDENT;
        dir_stack.pushHead(Dir::open(notnil(path)? path.copy() : Path::from("."_str, &bump)));

        assert(node->next); // IDK if this will ever happen??
        Part* next_part = node->next->data;

        if (isnil(path))
        {
          // push the current directory to be searched 
          // I DONT KNOW if this is a good idea it just solves 
          // an edge case like **/src/**/*.cpp
          entry_stack.pushTail({node->next, Path::from(""_str, &bump)});
        }

        b8 recurse = false;
        while (!dir_stack.isEmpty())
        {
          if (recurse)
          {
            INFO("recursing into ", dir_path_stack.head(), "\n");
            Dir d = Dir::open(dir_path_stack.head());
            if (isnil(d))
            {
              dir_path_stack.popHead();
              if (dir_path_stack.isEmpty())
                break;
            }
            dir_stack.pushHead(d);
            recurse = false;
          }

          Dir* dir = &dir_stack.head();
          INFO("iterating path ", dir_path_stack.head(), "\n");

          StackArray<u8, 255> dirent;

          dirent.len = dir->next({dirent.arr, dirent.capacity()});
          if (dirent.len == -1)
            goto err;

          if (dirent.len == 0)
          {
            dir->close(); 

            INFO("out of entries\n");

            dir_path_stack.popHead();
            dir_stack.popHead();

            if (dir_path_stack.isEmpty())
              break;

            INFO("dir path is now ", dir_path_stack.head(), "\n");

            continue;
          }
          
          str s = str::from(dirent.asSlice());

          if (s == "."_str || s == ".."_str)
          {
            INFO("skipping entry ", s, " because it is either the current or parent directory\n");
            continue;
          }

          if (s.ptr[0] == '.')
          {
            INFO("skipping entry ", s, " because it is a dotfile\n");
            continue;
          }

          INFO("found entry ", s, "\n");

          Path dir_path = dir_path_stack.head();
          Path full = notnil(dir_path)? dir_path.copy().makeDir().append(s) : Path::from(s, &bump);

          INFO("fullpath is ", full, "\n");

          switch (next_part->kind)
          {
          case ConstantEntry:
            if (s == next_part->raw) // TODO(sushi) mark constant entries as merged so we can skip this check 
            {
              if (!callback(full))
                goto done;
            }
            break;

          case MatchEntry:
            if (Path::matches(s, next_part->raw))
            {
              if (!callback(full))
                goto done;
            }
            break;
          }

          if (full.isDirectory())
          {
            INFO("pushing dir ", full, "\n");
            entry_stack.pushTail({node->next, full.copy()});
            dir_path_stack.pushHead(full.copy());
            recurse = true;
          }
        }

      }
      break;
    }
  }

err: // TODO(sushi) maybe do some kinda error handling sometime idrk if i care for that yet
done:
  return;
}

}

#endif 

