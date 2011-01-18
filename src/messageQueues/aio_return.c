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

/*---------------------------------------------------------
  aio_return.c:
  
     - Ermittlung des Rueckgabewertes:
       --> -1 im Fehlerfall, sonst:
       --> Anzahl gelesener Bytes
     - anschliessendes Aushaengen des entsprechenden Kontrollblocks aus der globalen Liste
---------------------------------------------------------*/


size_t aio_return (struct aiocb *aiocbp)
{
    struct aiocb *localHead = HeadPtr;
    struct aiocb *predecessorCB = NULL;
    size_t retval = -1;


    /* Pointerzuweisung nicht erfolgreich bzw. globaler HeadPtr ungueltig */
    if (!localHead)
    {
        errno = EINVAL;
        return retval;
    }
    

    /* gesuchter Kontrollblock == erstes Listenelement? */
    if (localHead->aio_pid == aiocbp->aio_pid)
    {
        retval = localHead->aio_nbytes;

        /* War der urspruengliche aio_read()- bzw. aio_write-Aufruf erfolgreich? */
        if (retval == -1)
        {
            errno = localHead->aio_errno;
        }

        /* Hat Liste insgesamt mehr als ein Element? */
        if (aiocbp->aio_next)
        {
            HeadPtr = aiocbp->aio_next;
        }
        else
        {
            HeadPtr->aio_next = NULL;
        }
        
        return retval;
    }
        

    
    /* Gesuchtes Glied ist nicht erstes Element */
    while (localHead && (localHead->aio_pid != aiocbp->aio_pid))
    {
        predecessorCB = localHead;

        /* kann weitergeschaltet werden? */
        if (!(localHead = localHead->aio_next))
        {
            /* gesuchter Kontrollblock befindet sich nicht in der Liste */
            errno = EINVAL;
            return retval;
        }
    }

    /* Konnte Element letztendlich gefunden werden? */
    if (predecessorCB)
    {
        /* Aushaengen des entsprechenden Kontrollblocks aus der Liste heraus */
        predecessorCB->aio_next = localHead->aio_next;
        retval = localHead->aio_nbytes;
        
        /* War der urspruengliche aio_read()- bzw. aio_write-Aufruf erfolgreich? */
        if (retval == -1)
        {
            errno = localHead->aio_errno;
        }
    }


    return retval;
}
