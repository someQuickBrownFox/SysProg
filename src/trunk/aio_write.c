#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <sys/unistd.h>
#include <sys/msg.h>

#include "aio.h"
#include "aioinit.h"

#define WSCHLUESSEL (key_t) 38266095

int _sigrcvd;

void aio_write_sighandler(int sig_nr)
{
    signal(sig_nr, aio_write_sighandler);
    _sigrcvd = sig_nr;
#ifdef DEBUG
    printf("aio_write_sighandler: Child ready for reading data\n");
    fflush(stdout);
#endif
}

int aio_write(struct aiocb *aiocbp)
{
    struct aiocb * headold;
    sig_t          sigold1;
    sig_t          sigold2;
    pid_t          sohn;
    int            pfd[2];
    char           pidbuff1[24];
    char           pidbuff2[24];
    int            success = 0;
    int            cdupfd;
    pid_t          ppid;
    int            write_msqid;
    struct msgbuf  wmsgbuf;
    size_t         anz, i, j;
    int            fflags;

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

    if ((fflags & O_WRONLY) != O_WRONLY  && (fflags & O_RDWR) != O_RDWR)
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

    if (pipe(pfd) == -1)
    {
        errno = EAGAIN;
        return -1;
    }

    // using the USR1 signal here for synchronizing could be a problem, if the aiosrv-operation is completed
    // before this routine finishes.
    if ((sigold1 = signal(SIGUSR1, aio_write_sighandler)) == SIG_ERR)
    {
        // should not happen
        errno = EINVAL;
        return -1;
    }

    if ((sigold2 = signal(SIGUSR2, aio_write_sighandler)) == SIG_ERR)
    {
        // should not happen
        errno = EINVAL;
        return -1;
    }

    switch (sohn = fork())
    {
        case -1:
            errno = EAGAIN;
            return -1;
        case 0: // child process
            ppid = getppid();
            switch (fork())
            {
                case -1:
                    kill(ppid, SIGUSR2);
                    while (close(pfd[0]) == -1 && errno == EINTR) {} // repeat it
                    exit(1);
                    break;
                case 0: //child child process
                    while (close(pfd[0]) == -1 && errno == EINTR) {} // repeat it
                    while (close(pfd[1]) == -1 && errno == EINTR) {} // repeat it

                    if ((write_msqid = msgget(WSCHLUESSEL, IPC_CREAT | 0600)) == -1)
                    {
                        kill(ppid, SIGUSR2);
                        exit(1);
                    }

                    anz = (int)(aiocbp->aio_nbytes / PLEN + 1);

                    wmsgbuf.mtype = 1;
                    for (i = 0; i < anz; i++)
                    {
                        for (j = 0; j < PLEN &&  i * PLEN + j < aiocbp->aio_nbytes; j++)
                        {
                            wmsgbuf.mtext[j] = ((char*)aiocbp->aio_buf)[i * PLEN + j];
                        }
                        if (msgsnd(write_msqid, &wmsgbuf, sizeof(struct msgbuf), 0) == -1)
                        {
                            kill(ppid, SIGUSR2);
                            exit(1);
                        }
                    }
                    exit(0);
                    break;
                default: //child process
                    while (close(pfd[1]) == -1 && errno == EINTR) {} // repeat it
                    while (close(0)      == -1 && errno == EINTR) {} // repeat it

                    if ((cdupfd = dup(pfd[0])) == -1 && errno == EMFILE)
                    {
                        // maximum number of file descriptors reached
                        kill(ppid, SIGUSR2);
                        // error would mean, that the parent is either already
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

                    //schreibe VatperprocessID in pidbuff
                    if (aiocbp->aio_lio_opcode == O_WRITE)
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

                    // this should be unreachable
                    kill(getppid(), SIGUSR2);
                    while (close(pfd[0]) == -1 && errno == EINTR) {} // repeat it
                    exit(1);
                    break;
            }
            break; 
        default:
            while (close(pfd[0]) == -1 && errno == EINTR) {} // repeat it
            pause();
            errno = 0;

            aiocbp->aio_pid        = sohn;
            aiocbp->aio_errno      = EINPROGRESS;
            aiocbp->aio_lio_opcode = O_WRITE;

#ifdef DEBUG
            printf("aio_write: aiocbp->aio_lio_opcode = %d\n", aiocbp->aio_lio_opcode);
#endif

            if (write(pfd[1], aiocbp, sizeof(struct aiocb)) == -1)
            {
                errno = EAGAIN;
                success = -1;
            }

            pause();
            
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

            kill(sohn, SIGUSR1);

            if (_sigrcvd != SIGUSR1)
            {
                errno = EAGAIN;
                success = -1;
            }

            while (close(pfd[1]) == -1 && errno == EINTR) {} // repeat it

            headold = HeadPtr;
            HeadPtr = aiocbp;
            HeadPtr->aio_next = headold;

            break;
    }

    return success;
}

