#include "FileTask.h"

/* ----------------------------------------------------------------------------
 */
b8 FileTask::init(String path)
{
  fs::Path canonical = {};
  if (!canonical.init())
    return false;

  u8* scan = path.begin();
  u8* part_start = scan;
  u8* end = path.end();

  for (;scan < path.end(); scan += 1)
  {
    switch (scan[0])
    {
      case '/':
      case '\\':
        {
          if (canonical.buffer.asStr().last() != '/')
            canonical.append('/');
        }
        break;

      case '.':
        if (scan < end - 2 &&
            scan[1] == '.' && (scan[2] == '/' || scan[2] == '\\'))
        {
          // ..
          canonical.removeBasename();
          scan += 1;
        }
        else if (scan < end - 1 && (scan[1] == '/' || scan[1] == '\\'))
        {
          // .
          break;
        }
        else
        {
          canonical.append('.');
        }
        break;

      default:
        canonical.append(*scan);
        break;
    }
  }

  if (!Task::init(canonical.buffer.asStr()))
    return false;

  canonical.destroy();
  
  return true;
}
