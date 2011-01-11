#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <string.h>

#include "aioinit.h"
#include "aio.h"


int aio_error (struct aiocb *aiocbp)
{
    struct aiocb *localHead = HeadPtr;
    if (!localHead) { return -1;}
    while (localHead->aio_pid != aiocbp->aio_pid)
    {

        if ((localHead = localHead->aio_next))
        {}
        else
        {
            return -1;
        }
    }
    return localHead->aio_errno;
}
