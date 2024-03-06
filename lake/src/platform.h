/*
 *
 *  Platform specific functions.
 *
 */

#include "common.h"

namespace platform
{

// read the file at path into a buffer
// use mem.allocate so we can use mem.free to free
u8* read_file(str path);

}
