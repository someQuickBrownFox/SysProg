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
    struct aiocb *lastCB = NULL;
    size_t retval = -1;


    /* ungueltiger Pointer */
    if (!localHead) {
        return retval;
    }
    

    /* erstes Element? */
    if (localHead->aio_pid == aiocbp->aio_pid) {
        retval = localHead->aio_nbytes;

        if (aiocbp->aio_next) {
            HeadPtr = aiocbp->aio_next;
        } else {
            HeadPtr->aio_next = NULL;
        }
        
        return retval;
    }
        

    
    /* Gesuchtes Glied ist nicht erstes Element */
    while (localHead && (localHead->aio_pid != aiocbp->aio_pid)) {
        lastCB = localHead;
        
        if (!(localHead = localHead->aio_next)) {
            return retval;
        }
    }

    if (lastCB) {
        lastCB->aio_next = localHead->aio_next;
    }


    retval = localHead->aio_nbytes;

    return retval;
}
