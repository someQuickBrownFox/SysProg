#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <sys/unistd.h>

#include "aio.h"
#include "aio_util.h"

void lio_listio_sighandler(int signo)
{
    signal(signo, lio_listio_sighandler);
}

int lio_listio(int mode, struct aiocb * list[], int nent, struct sigevent *sig)
{
    // The signal should indicate the finished execution of ALL control blocks!
    // This means, we have to fork, if this function is called in non-blocking manner.

    struct aiocb * aicbp;
    int            retval = 0;
    int            i, cnt = 0;
    pid_t          cpid   = 0;
    sig_t          oldsig;

    if (list == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    if (mode != LIO_WAIT && mode != LIO_NOWAIT)
    {
        errno = EINVAL;
        return -1;
    }

    if (mode == LIO_NOWAIT && sig != NULL)
    {
        switch (cpid = fork())
        {
            case -1:
                errno = EAGAIN;
                retval = -1;
                break;
            case 0:
                if ((oldsig = signal(SIGUSR1, lio_listio_sighandler)) == SIG_ERR)
                {
                    // should not happen
                }

                while (cnt != nent)
                {
                    pause();
                    errno = 0;
                    aio_pdebug("%s (%d): One of our dispatched control blocks"
                               " is completed!\n",
                               __FILE__, __LINE__);
                    cnt ++;
                }

                if (kill(getppid(), sig->sigev_signo) == -1)
                {
                    // this block should never be reached. really!!
                }

                if (signal(SIGUSR1, oldsig) == SIG_ERR)
                {
                    // should not happen
                }

                exit(0);
                break;
            default:
                for (i = 0; i < nent; i ++)
                {
                    aicbp = list[i];
                    if (aicbp != NULL)
                    {
                        // We need to temporarely abuse the aio_errno for storing
                        // a pid, so that aio_read()/aio_write() know, what PIDs
                        // else it has to tell to aiosrv. This is needed for
                        // recognizing a finished process of a control block
                        // dispatched by this function. This is only needed
                        // in LIO_NOWAIT mode. One received signal means
                        // one completed control block. See `case 0:` above.
                        aicbp->aio_errno = cpid; 
                    }
                }
                break;
        }
    }

    for (i = 0; i < nent; i ++)
    {
        aicbp = list[i];
        if (aicbp != NULL)
        {
            aio_pdebug("%s (%d): Dispatching a control block...\n",
                       __FILE__, __LINE__);

            if (aicbp->aio_lio_opcode == O_READ && aio_read(aicbp) == -1)
            {
                if (mode == LIO_NOWAIT)
                {
                    aio_pdebug("%s (%d): Inform the lio_listio() forked child, "
                               "that one control block is not possible to be "
                               "processed\n",
                               __FILE__, __LINE__);
                    if (kill(cpid, SIGUSR1) == -1)
                    {
                        aio_perror("%s (%d): Failed signaling failed processing "
                                   "of an erroneous control block to the child",
                                   __FILE__, __LINE__);
                    }
                }
                retval = -1;
            }
            else if (aicbp->aio_lio_opcode == O_WRITE && aio_write(aicbp) == -1)
            {
                if (mode == LIO_NOWAIT)
                {
                    aio_pdebug("%s (%d): Inform the lio_listio() forked child, that "
                               "one control block is not possible to be processed\n",
                               __FILE__, __LINE__);
                    if (kill(cpid, SIGUSR1) == -1)
                    {
                        aio_perror("%s (%d): Failed signaling failed processing "
                                   "of an erroneous control block to the child",
                                   __FILE__, __LINE__);
                    }
                }
                retval = -1;
            }
            else if (aicbp->aio_lio_opcode != O_READ && aicbp->aio_lio_opcode != O_WRITE)
            {
                aio_pdebug("%s (%d): Invalid opcode found\n",
                           __FILE__, __LINE__);
                errno = EINVAL;
                retval = -1;
            }
        }
    }

    switch (mode)
    {
        case LIO_NOWAIT:
            aio_pdebug("%s (%d): Running in non-blocking mode\n",
                       __FILE__, __LINE__);
            break;
        case LIO_WAIT:
            aio_pdebug("%s (%d): Running in blocking mode\n",
                       __FILE__, __LINE__);
            // according to the original aio specs we have to wait until ALL
            // scheduled operations are completed.
            /*
            while (flag == 0)
            {
                for (i = 0; i < nent; i ++)
                {
                    aicbp = list[i];
                    if (aicbp == NULL)
                    {
                        break;
                    }

                    if (aicbp->aio_errno == EINPROGRESS)
                    {
                        flag = 1;
                    }

                    if (aicbp->aio_errno != EINPROGRESS || aicbp->aio_errno != 0)
                    {
                        retval = -1;
                    }
                }
            }
            */

            // The following commented code may be a version, where the
            // application is blocked until ALL control blocks are processed
            // successfully. If an error occurs, the function returns 
            // immediatelly with a return value of -1. I'm unsure, if this one
            // is the right version. At least according to the manual
            // page of the original aio implementation this would work more
            // correctly.
            while (cnt != nent)
            {
                cnt = 0;
                for (i = 0; i < nent; i ++)
                {
                    aicbp = list[i];
                    if (aicbp != NULL && aicbp->aio_errno != EINPROGRESS)
                    {
                        cnt ++;
                        if (aicbp->aio_errno != 0)
                        {
                            retval = -1;
                        }
                    }
                }
            }
            break;
    }

    return retval;
}

