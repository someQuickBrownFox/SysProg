#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <sys/unistd.h>
//#include <sys/stat.h>
#include <fcntl.h>

#include "aio.h"
#include "aioinit.h"
int _sigrcvd;

#define DEBUG 1


void aio_read_sighandler(int sig_nr)
{
    signal(sig_nr, aio_read_sighandler);
    _sigrcvd = sig_nr;
#ifdef DEBUG
    printf("aio_read_sighandler: Child ready for reading the control block\n");
    fflush(stdout);
#endif
}

int aio_read(struct aiocb * aiocbp)
{
    sig_t          sigold1; //bei max osx... unter ubuntu gar keine definition des zeigers gefunden(singal.h)
    sig_t          sigold2;
    struct aiocb * headold;
    int            pfd[2]; //pipe file descriptor [0] Pipe-Ausgang [1] Pipe-Eingang
    pid_t          cpid;   //child processid
    char           pidbuff1[12];
    char           pidbuff2[12];
    int            success = 0;
    int            cdupfd;
    int            fflags;
    /*struct stat    fstat;*/

    if (aiocbp == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    if ((fflags = fcntl(aiocbp->aio_fildes, F_GETFL)) == -1 && errno == EBADF)
    {
        // errno = EBADF;
        return -1;
    }

    if ((fflags & O_RDONLY) != O_RDONLY && (fflags & O_RDWR) != O_RDWR)
    {
        errno = EBADF;
        return -1;
    }

    if (aiocbp->aio_offset < 0 || aiocbp->aio_nbytes <= 0)
    {
        errno = EINVAL;
        return -1;
    }

    /*
    if (fstat(aiocbp->aio_fildes, &fstat) == -1)
    {
        // fstat may set errno to EBADF, EOVERFLOW or EIO
        return -1;
    }

    if (aiocbp->aio_offset >= fstat.st_size)
    {
        errno = EOVERFLOW;
        return -1;
    }
    */

    // create unnamed pipe
    if (pipe(pfd) == -1)
    {
        errno = EAGAIN;
        return -1;
    }

    // we need this signal for synchronizing the data transfer from parent to child.
    // using the USR1 signal here for synchronizing could be a problem, if the aiosrv-operation is completed
    // before this routine finishes.
    if ((sigold1 = signal(SIGUSR1, aio_read_sighandler)) == SIG_ERR)
    {
        // should not happen
        errno = EINVAL;
        return -1;
    }

    if ((sigold2 = signal(SIGUSR2, aio_read_sighandler)) == SIG_ERR)
    {
        // should not happen
        errno = EINVAL;
        return -1;
    }

    // fork
    switch (cpid = fork())
    {
        case -1:
            errno = EAGAIN;
            return -1;
        case 0: // child process
            // pipe auf stdin umleiten und den nicht gebrauchten pipe-eingang schließen
            while (close(pfd[1]) == -1 && errno == EINTR) {} // repeat it
            while (close(0)      == -1 && errno == EINTR) {} // repeat it

            if ((cdupfd = dup(pfd[0])) == -1 && errno == EMFILE)
            {
                // maximum number of file descriptors reached
                kill(getppid(), SIGUSR2); // error would mean, that the parent is either already
                                          // dead, than we can ignore it and die, or that the
                                          // signal is wrong (it's definitely not) or that the parent
                                          // process is running under a different user account (should
                                          // NEVER be the case, except for setuid)
                while (close(pfd[0]) == -1 && errno == EINTR) {} // repeat it
                exit(1);
            }

            if (cdupfd != 0)
            {
                perror("tux/beasty doesn't like to override the stdin");
                kill(getppid(), SIGUSR2);
                while (close(pfd[0]) == -1 && errno == EINTR) {} // repeat it
                exit(1);
            }

            if (aiocbp->aio_lio_opcode == O_READ)
            {
                // it was a LIO_NOWAIT list-call! we have to tell aiosrv, which pid numbers it has to signal after a operation is finished.
                sprintf(pidbuff1, "%d", getppid());
                sprintf(pidbuff2, "%d", aiocbp->aio_errno);
                execlp("aiosrv", "aiosrv", pidbuff1, pidbuff2, NULL);
            }
            else
            {
                sprintf(pidbuff1, "%d", getppid());
                execlp("aiosrv", "aiosrv", pidbuff1, NULL);
            }

            // should not be reached, but could mean an error case
            kill(getppid(), SIGUSR2);
            while (close(pfd[0]) == -1 && errno == EINTR) {} // repeat it
            exit(1);
            break;
        default:
            // close pipe input
            while (close(pfd[0]) == -1 && errno == EINTR) {} // repeat it
            pause(); // wait until the child is ready for receiving data
            errno = 0;

            aiocbp->aio_pid        = cpid;
            aiocbp->aio_lio_opcode = O_READ;
            aiocbp->aio_errno      = EINPROGRESS;

#ifdef DEBUG
            printf("aio_read: aiocbp->aio_pid == %d\n", aiocbp->aio_pid);
            printf("aio_read: aiocbp->aio_lio_opcode = %d\n", aiocbp->aio_lio_opcode);
#endif

            if (write(pfd[1], aiocbp, sizeof(struct aiocb)) == -1)
            {
                errno = EAGAIN;
                success = -1;
            }

            pause();
            if (_sigrcvd != SIGUSR1)
            {
                errno = EAGAIN;
                success = -1;
            }

            while (close(pfd[1]) == -1 && errno == EINTR) {} // repeat it

            // enqueue the control block if stuff was going right
            headold = HeadPtr;
            HeadPtr = aiocbp;
            HeadPtr->aio_next = headold;

            break;
    }

    if (signal(SIGUSR1, sigold1) == SIG_ERR)
    {
        // should not happen
        perror("failed resetting the USR1 signal service routine");
        errno = EINVAL;
        return -1;
    }

    if (signal(SIGUSR2, sigold2) == SIG_ERR)
    {
        // should not happen
        perror("failed resetting the USR2 signal service routine");
        errno = EINVAL;
        return -1;
    }

    return success;
}

