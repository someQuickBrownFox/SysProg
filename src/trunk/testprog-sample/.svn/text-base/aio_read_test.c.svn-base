#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <sys/unistd.h>

#include <fcntl.h>

#define DEBUG
#include "../aio.h"

int main(int argc, char* argv[])
{
    //---------------Test aio controllblock---------------------------
    struct aiocb element;
    char buffer[32768];
    element.aio_fildes  = open("./test.txt", O_RDONLY, 0);
    element.aio_offset  = 0;
    element.aio_buf     = buffer;
    element.aio_nbytes  = sizeof(buffer);
    element.aio_reqprio = 0;
    //-----------------------------------------------------------------

    printf("Starting the process...\n");
    if (aio_read(&element) == -1) {
        perror("failed.");
    }
    printf("Sent.\n");
    return 0;
}

