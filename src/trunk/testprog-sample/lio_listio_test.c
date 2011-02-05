#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <sys/unistd.h>

#include <fcntl.h>

#include "../aio.h"

void test_sighandler(int signo)
{
    signal(signo, test_sighandler);
    printf("lio_listio_test: Received signal %d", signo);
}

int main(/*int argc, char* argv[]*/)
{
    struct aiocb * cb[6]; // 0 = read,  1 = read, 2 = write,
                          // 3 = write, 4 = read, 5 = write 
    struct aiocb cb0, cb1, cb2, cb3, cb4, cb5;
    struct sigevent sig;
    int i;

    cb[0] = &cb0;
    cb[1] = &cb1;
    cb[2] = &cb2;
    cb[3] = &cb3;
    cb[4] = &cb4;
    cb[5] = &cb5;

    for (i = 0; i < 6; i ++)
    {
        cb[i]->aio_buf = NULL;
    }

    if (signal(SIGUSR2, test_sighandler) == SIG_ERR)
    {
        perror("Error setting signal handler\n");
        return 1;
    }

    aio_init();

    sig.sigev_signo = SIGUSR2;
    
    int fd1 = open("./lio_test1.txt", O_RDONLY, 0);
    int fd2 = open("./lio_test2.txt", O_RDWR /*O_WRONLY*/, 0);
    int fd3 = open("./lio_test3.txt", O_RDWR,   0);

    cb[0]->aio_lio_opcode = O_READ;
    cb[1]->aio_lio_opcode = O_READ;
    cb[2]->aio_lio_opcode = O_WRITE;
    cb[3]->aio_lio_opcode = O_WRITE;
    cb[4]->aio_lio_opcode = O_READ;
    cb[5]->aio_lio_opcode = O_WRITE;
/*    for (i = 0; i < 6; i ++)
    {
        cb[i]->aio_lio_opcode = O_READ;
    }*/

    cb[0]->aio_fildes = fd1;
    cb[1]->aio_fildes = fd1;
    cb[2]->aio_fildes = fd2;
    cb[3]->aio_fildes = fd2;
    cb[4]->aio_fildes = fd3;
    cb[5]->aio_fildes = fd3;

    cb[0]->aio_offset = 0;
    cb[1]->aio_offset = 0;
    cb[2]->aio_offset = 0;
    cb[3]->aio_offset = 0;
    cb[4]->aio_offset = 0;
    cb[5]->aio_offset = 0;

    // A non-null buffer is only required for for write operations.
    cb[2]->aio_buf = "Hello ";
    cb[3]->aio_buf = "World";
    cb[5]->aio_buf = "Foobar ";

    cb[0]->aio_nbytes = 6;
    cb[1]->aio_nbytes = 5;
    cb[2]->aio_nbytes = 6;
    cb[3]->aio_nbytes = 5;
    cb[4]->aio_nbytes = 4;
    cb[5]->aio_nbytes = 7;

    lio_listio(LIO_NOWAIT, cb, 6, &sig);
    printf("BAMM\n");
    fflush(stdout);

    for (i = 0; i < 6; i ++)
    {
        while (aio_error(cb[i]) == EINPROGRESS); // wait until all are ready
    }

    printf("Ready.\n");
    aio_cleanup();

    return 0;
}
