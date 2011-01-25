#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <sys/unistd.h>

#include "aio.h"

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
    // int flag = 0;
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
                // I guess, this is still incorrect... The process does not know about changes in the values
                // of the parent program... Any advice?
                if ((oldsig = signal(SIGUSR1, lio_listio_sighandler)) == SIG_ERR)
                {
                    // should not happen
                }

                while (cnt != nent)
                {
                    pause();
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
                        aicbp->aio_errno = cpid; // temporary abuse of the errno
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
            switch (aicbp->aio_lio_opcode)
            {
                case O_READ:
                    retval = (aio_read(aicbp) == -1) ? -1 : retval;
                    break;
                case O_WRITE:
                    retval = (aio_write(aicbp) == -1) ? -1 : retval;
                    break;
                default:
                    retval = -1;
                    errno = EINVAL;
                    return retval;
            }
        }

        if (aicbp->aio_lio_opcode == O_READ && aio_read(aicbp) == -1)
        {
            retval = -1;
        }
        else if (aicbp->aio_lio_opcode == O_WRITE && aio_write(aicbp) == -1)
        {
            retval = -1;
        }
    }

    switch (mode)
    {
        case LIO_NOWAIT:
            break;
        case LIO_WAIT:
            // according to the original aio specs we have to wait until ALL scheduled operations are completed.
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

            // The following commented code may be a version, where the application is blocked
            // until ALL control blocks are processed successfully. If an error occurs, the 
            // function returns immediatelly with a return value of -1.
            // I'm unsure, if this one is the right version. At least according to the manual
            // page of the original aio implementation this would work more correctly.
            while (cnt != nent)
            {
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

