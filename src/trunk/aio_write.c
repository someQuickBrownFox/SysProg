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
#include "aio_util.h"

#define WSCHLUESSEL (key_t) 38266095

int _sigrcvd;
int _sigusr1cnt;
int _sigusr2cnt;

void aio_write_sighandler(int signo)
{
    signal(signo, aio_write_sighandler);
    _sigrcvd = signo;
    if (signo == SIGUSR1)
    {
        _sigusr1cnt ++;
    }
    else if (signo == SIGUSR2)
    {
        _sigusr2cnt ++;
    }

    aio_pdebug("%s (%d): Received signal %d\n",
               __FILE__, __LINE__,
               signo);
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
    int            fflags;
    size_t         chunk_len = PLEN;
    int            chunk_cnt = 0;

    _sigusr1cnt = 0;
    _sigusr2cnt = 0;

    if (aiocbp == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    if ((fflags = fcntl(aiocbp->aio_fildes, F_GETFL)) == -1 && errno == EBADF)
    {
        aio_pdebug("%s (%d): Getting flags of file descriptor failed\n",
                   __FILE__, __LINE__);
        // errno = EBADF;
        return -1;
    }

    if ((fflags & O_WRONLY) != O_WRONLY  && (fflags & O_RDWR) != O_RDWR)
    {
        aio_pdebug("%s (%d): File is not opened for writing\n", __FILE__,
                   __LINE__);
        errno = EBADF;
        return -1;
    }

    if (aiocbp->aio_offset < 0 || aiocbp->aio_nbytes <= 0)
    {
        aio_pdebug("%s (%d): aiocbp->aio_offset (value %d) or "
                   "aiocbp->aio_nbytes (value %d) is negative\n",
                   __FILE__, __LINE__,
                   aiocbp->aio_offset,
                   aiocbp->aio_nbytes);
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

    // using the USR1 signal here for synchronizing could be a problem, if the
    // aiosrv-operation is completed before this routine finishes.
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
                    while (close(pfd[0]) == -1 && errno == EINTR) {} // repeat
                    exit(1);
                    break;
                case 0: //child child process
                    while (close(pfd[0]) == -1 && errno == EINTR) {} // repeat
                    while (close(pfd[1]) == -1 && errno == EINTR) {} // repeat

                    if ((write_msqid = msgget(WSCHLUESSEL, IPC_CREAT | 0600))
                            == -1)
                    {
                        aio_perror("%s (%d): Failed getting the message queue",
                                   __FILE__, __LINE__);
                        kill(ppid, SIGUSR2);
                        exit(1);
                    }


                    while (chunk_cnt * chunk_len < aiocbp->aio_nbytes)
                    {
                        if (aiocbp->aio_nbytes < (chunk_cnt + 1) * chunk_len)
                        {
                            chunk_len = aiocbp->aio_nbytes - chunk_cnt *
                                chunk_len;
                        }

                        wmsgbuf.mtype = getpid();
                        memcpy(wmsgbuf.mtext,
                               aiocbp->aio_buf,
                               chunk_len);
                        aio_pdebug("%s (%d): Sending a message...\n",
                                   __FILE__, __LINE__);
                        if (msgsnd(write_msqid, &wmsgbuf,
                                   sizeof(struct msgbuf), 0) == -1)
                        {
                            aio_perror("%s (%d): Failed sending a message",
                                       __FILE__, __LINE__);
                            kill(ppid, SIGUSR2);
                            exit(1);
                        }

                        chunk_cnt ++;
                    }
                    aio_pdebug("%s (%d): Ready.\n",
                               __FILE__, __LINE__);
                    exit(0);
                    break;
                default: //child process
                    while (close(pfd[1]) == -1 && errno == EINTR) {} // repeat
                    while (close(0)      == -1 && errno == EINTR) {} // repeat

                    if ((cdupfd = dup(pfd[0])) == -1 && errno == EMFILE)
                    {
                        aio_pdebug("%s (%d): Maximum number of file "
                                   "descriptors reached",
                                  __FILE__, __LINE__);
                        kill(ppid, SIGUSR2);
                        // error would mean, that the parent is either already
                        // dead, than we can ignore it and die, or that the
                        // signal is wrong (it's definitely not) or that the 
                        // parent process is running under a different user
                        // account (should NEVER be the case, except for
                        // setuid)
                        while (close(pfd[0]) == -1 && errno == EINTR) {} 
                        exit(1);
                    }

                    if (cdupfd != 0)
                    {
                        aio_perror("%s (%d): Tux or Beasty does not like to "
                                   "override the stdin.",
                                   __FILE__, __LINE__);
                        kill(getppid(), SIGUSR2);
                        while (close(pfd[0]) == -1 && errno == EINTR) {}
                        exit(1);
                    }

                    //schreibe VatperprocessID in pidbuff
                    if (aiocbp->aio_lio_opcode == O_WRITE && aiocbp->aio_errno > 0)
                    {
                        // it was a LIO_NOWAIT list-call! we have to tell
                        // aiosrv, which pid numbers it has to signal after a
                        // operation is finished.
                        sprintf(pidbuff1, "%d", getppid());
                        sprintf(pidbuff2, "%d", aiocbp->aio_errno);
                        aio_pdebug("%s (%d): Executing `aiosrv %s %s`...\n",
                                   __FILE__, __LINE__,
                                   pidbuff1, pidbuff2);
                        execlp("aiosrv", "aiosrv", pidbuff1, pidbuff2, NULL);
                    }
                    else
                    {
                        sprintf(pidbuff1, "%d", getppid());
                        aio_pdebug("%s (%d): Executing `aiosrv %s`...\n",
                                   __FILE__, __LINE__,
                                   pidbuff1);
                        execlp("aiosrv", "aiosrv", pidbuff1, NULL);
                    }

                    // this should be unreachable
                    kill(getppid(), SIGUSR2);
                    while (close(pfd[0]) == -1 && errno == EINTR) {} // repeat
                    exit(1);
                    break;
            }
            break; 
        default:
            while (close(pfd[0]) == -1 && errno == EINTR) {} // repeat it
           
            if (_sigusr1cnt == 0)
            {
                aio_pdebug("%s (%d): Waiting for child response...\n",
                           __FILE__, __LINE__); 
                pause();
                errno = 0;
                aio_pdebug("%s (%d): Continuing\n",
                           __FILE__, __LINE__);
            }
            else
            {
                aio_pdebug("%s (%d): Child is ready\n",
                           __FILE__, __LINE__);
            }

            aiocbp->aio_pid        = sohn;
            aiocbp->aio_errno      = EINPROGRESS;
            aiocbp->aio_lio_opcode = O_WRITE;

            aio_pdebug("%s (%d): aiocbp->aio_pid = %d\n",
                       __FILE__, __LINE__,
                       aiocbp->aio_pid);
            aio_pdebug("%s (%d): aiocbp->aio_lio_opcode = %d\n",
                       __FILE__, __LINE__,
                       aiocbp->aio_lio_opcode);

            if (write(pfd[1], aiocbp, sizeof(struct aiocb)) == -1)
            {
                aio_perror("%s (%d): Sending the control block to the child "
                           "failed.",
                           __FILE__, __LINE__); 
                errno = EAGAIN;
                success = -1;
            }

            if (_sigusr1cnt == 1 || _sigusr2cnt == 0)
            {
                aio_pdebug("%s (%d): Waiting for child response...\n",
                           __FILE__, __LINE__);
                pause();
                errno = 0;
                aio_pdebug("%s (%d): Continuing\n",
                           __FILE__, __LINE__);
            }
            else
            {
                aio_pdebug("%s (%d): Child is ready\n",
                           __FILE__, __LINE__);
            }

            if (_sigusr2cnt != 0)
            {
                aio_pdebug("%s (%d): Child died unexpectedly!\n",
                           __FILE__, __LINE__);
                errno = EAGAIN;
                success = -1;
            }
            
            if (signal(SIGUSR1, sigold1) == SIG_ERR)
            {
                // should not happen
                aio_perror("%s (%d): Failed resetting the USR1 signal service"
                           " routine",
                           __FILE__, __LINE__);
                errno = EINVAL;
                return -1;
            }

            if (signal(SIGUSR2, sigold2) == SIG_ERR)
            {
                // should not happen
                aio_perror("%s (%d): Failed resetting the USR2 signal service"
                           " routine",
                           __FILE__, __LINE__);
                errno = EINVAL;
                return -1;
            }

            aio_pdebug("%s (%d): Let the child do the work...\n",
                       __FILE__, __LINE__);
            if (kill(sohn, SIGUSR1) == -1)
            {
                aio_perror("%s (%d): Could not send USR1 to the child",
                           __FILE__, __LINE__);
            }

            while (close(pfd[1]) == -1 && errno == EINTR) {} // repeat it

            aio_pdebug("%s (%d): Enqueing the control block to the global "
                       "list\n",
                       __FILE__, __LINE__);
            headold = HeadPtr;
            HeadPtr = aiocbp;
            HeadPtr->aio_next = headold;

            break;
    }

    return success;
}

