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
#include "aio_util.h"

int _sigrcvd;
int _sigusr1cnt;
int _sigusr2cnt;

void aio_read_sighandler(int signo)
{
    signal(signo, aio_read_sighandler);
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

int aio_read(struct aiocb * aiocbp)
{
    sig_t          sigold1; 
    sig_t          sigold2;
    struct aiocb * headold;
    int            pfd[2]; // pipe fd: [0] pipe out; [1] pipe in
    pid_t          cpid;   // child pid
    char           pidbuff1[24];
    char           pidbuff2[24];
    int            success = 0;
    int            cdupfd;
    int            fflags;
    /*struct stat    fstat;*/
    
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

    if ((fflags & O_RDONLY) != O_RDONLY && (fflags & O_RDWR) != O_RDWR)
    {
        aio_pdebug("%s (%d): File is not opened for reading\n", __FILE__,
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
    // does not work, fstat not defined... (?!)
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

    // We need this signal for synchronizing the data transfer from parent to
    // child. Using the USR1 signal here for synchronizing could be a problem,
    // if the aiosrv-operation is completed before this routine finishes.
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
            // pipe auf stdin umleiten und den nicht gebrauchten pipe-eingang
            // schließen
            while (close(pfd[1]) == -1 && errno == EINTR) {} // repeat it
            while (close(0)      == -1 && errno == EINTR) {} // repeat it

            if ((cdupfd = dup(pfd[0])) == -1 && errno == EMFILE)
            {
                aio_pdebug("%s (%d): Maximum number of file descriptors "
                           "reached",
                           __FILE__, __LINE__);
                // maximum number of file descriptors reached
                kill(getppid(), SIGUSR2);
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
                aio_perror("%s (%d): tux/beasty doesn't like to override the "
                           "stdin",
                           __FILE__, __LINE__);
                kill(getppid(), SIGUSR2);
                while (close(pfd[0]) == -1 && errno == EINTR) {} // repeat it
                exit(1);
            }

            if (aiocbp->aio_lio_opcode == O_READ && aiocbp->aio_errno > 0)
            {
                // it was a LIO_NOWAIT list-call! we have to tell aiosrv,
                // which pid numbers it has to signal after a operation is
                // finished.
                sprintf(pidbuff1, "%d", getppid());
                sprintf(pidbuff2, "%d", aiocbp->aio_errno);
                aio_pdebug("%s (%d): Executing `aiosrv %s %s`\n",
                           __FILE__, __LINE__,
                           pidbuff1, pidbuff2);
                execlp("aiosrv", "aiosrv", pidbuff1, pidbuff2, NULL);
            }
            else
            {
                sprintf(pidbuff1, "%d", getppid());
                aio_pdebug("%s (%d): Executing `aiosrv %s`\n",
                           __FILE__, __LINE__,
                           pidbuff1);
                execlp("aiosrv", "aiosrv", pidbuff1, NULL);
            }

            // should not be reached, but could mean an error case
            aio_pdebug("%s (%d): Failed executing aiosrv\n",
                       __FILE__, __LINE__);
            kill(getppid(), SIGUSR2);
            while (close(pfd[0]) == -1 && errno == EINTR) {} // repeat it
            exit(1);
            break;
        default:
            // close pipe input
            while (close(pfd[0]) == -1 && errno == EINTR) {} // repeat it

            if (_sigusr1cnt == 0)
            {
                aio_pdebug("%s (%d): Waiting for child response...\n",
                           __FILE__, __LINE__);
                pause(); // wait until the child is ready for receiving data
                errno = 0;
                aio_pdebug("%s (%d): Continuing\n", __FILE__, __LINE__);
            }
            else
            {
                aio_pdebug("%s (%d): Child is ready\n",
                           __FILE__, __LINE__);
            }

            aiocbp->aio_pid        = cpid;
            aiocbp->aio_lio_opcode = O_READ;
            aiocbp->aio_errno      = EINPROGRESS;

            aio_pdebug("%s (%d): aiocbp->aio_pid == %d\n",
                       __FILE__, __LINE__, aiocbp->aio_pid);
            aio_pdebug("%s (%d): aiocbp->aio_lio_opcode = %d\n",
                       __FILE__, __LINE__, aiocbp->aio_lio_opcode);

            if (write(pfd[1], aiocbp, sizeof(struct aiocb)) == -1)
            {
                aio_pdebug("%s (%d): Sending the control block to the child "
                           "failed.\n",
                           __FILE__, __LINE__);
                errno = EAGAIN;
                success = -1;
            }

            if (_sigusr1cnt == 1 && _sigusr2cnt == 0)
            {
                aio_pdebug("%s (%d): Waiting for child response...\n",
                           __FILE__, __LINE__);
                pause();
                errno = 0;
                aio_pdebug("%s (%d): Continuing\n", __FILE__, __LINE__);
            }
            else
            {
                aio_pdebug("%s (%d): Child is ready\n",
                           __FILE__, __LINE__);
            }

            if (_sigusr2cnt != 0)
            {
                aio_pdebug("%s (%d): Child died unexpectedly\n",
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
                           " routine", __FILE__, __LINE__);
                errno = EINVAL;
                return -1;
            }

            if (success == 0)
            {
                aio_pdebug("%s (%d): Let the child do the work...\n",
                           __FILE__, __LINE__);
                if (kill(cpid, SIGUSR1) == -1)
                {
                    aio_perror("%s (%d): Could not send USR1 to the child\n",
                               __FILE__, __LINE__);
                }
            }


            while (close(pfd[1]) == -1 && errno == EINTR) {} // repeat it

            // enqueue the control block if stuff was going right
            aio_pdebug("%s (%d): Enqueing the aiocbp to the global list\n",
                       __FILE__, __LINE__);
            headold = HeadPtr;
            HeadPtr = aiocbp;
            HeadPtr->aio_next = headold;

            break;
    }

    return success;
}

