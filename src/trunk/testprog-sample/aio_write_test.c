#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <sys/unistd.h>

#include <fcntl.h>

#include "../aio.h"

int main(int argc, char* argv[])
{
	int fd;
	struct aiocb element;
	char buffer[256];
	fd = open("./test.txt", O_WRONLY, 0);
	printf("FD: %d\n", fd);
	element.aio_fildes	= fd;
	element.aio_offset	= 0;
	element.aio_buf		= buffer;
	element.aio_nbytes	= sizeof(buffer);
	element.aio_reqprio	= 0;

	aio_write(&element);
	close(fd);
	return 0;
}

