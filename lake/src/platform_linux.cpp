#include "platform.h"
#include "lake.h"

#include "stdlib.h"
#include "stdio.h"

#include "fcntl.h"
#include "sys/stat.h"
#include "unistd.h"

namespace platform
{

u8* read_file(str path) 
{
	int fd = open((char*)path.s, O_RDONLY);

	if (fd == -1)
	{
		error(path, 0, 0, "failed to open file");
		perror("open");
		exit(1);
	}

	struct stat st;
	fstat(fd, &st);

	u8* buffer = (u8*)mem.allocate(st.st_size);

	ssize_t bytes_read = read(fd, buffer, st.st_size);
	
	if (bytes_read != st.st_size)
	{
		error(path, 0, 0, "failed to read entire file");
		perror("read");
		exit(1);
	}

	return buffer;
}

}
