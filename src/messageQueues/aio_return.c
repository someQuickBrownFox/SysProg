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


size_t aio_return (struct aiocb *aiocbp)
{
    struct aiocb *localHead = HeadPtr;
    struct aiocb *myCB = NULL;
    size_t retval = -1;


    if (!localHead) { return retval;}
    while (localHead->aio_pid != aiocbp->aio_pid)
    {
        myCB = localHead;
        if ((localHead = localHead->aio_next))
        {   }
        else
        {
            return retval;
        }
    }

    if (myCB)
    {
    myCB->aio_next = localHead->aio_next;
    }


    retval = localHead->aio_nbytes;

    return retval;
}
