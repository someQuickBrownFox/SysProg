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
  aio_error.c:
  
     - Ermittlung des Abschluß- bzw. Fehlerstatus:
       --> EINPROGRESS, falls in *aiocbp referierter Auftrag noch in Bearbeitung,
       --> 0, falls Auftrag erfolgreich abgeschlossen,
       --> errno der zugrundeliegenden Operation, falls Auftrag fehlerhaft abgeschlossen;
       --> -1, falls Fehler bei aio_error () selbst.
---------------------------------------------------------*/

int aio_error (struct aiocb *aiocbp)
{
    struct aiocb *localHead = HeadPtr;

    /* Pointerzuweisung nicht erfolgreich bzw. globaler HeadPtr ungueltig */
    if (!localHead)
    {
        return -1;
    }

    /* Suche passenden Kontrollblock */
    while (localHead->aio_pid != aiocbp->aio_pid)
    {
        if (!(localHead = localHead->aio_next))
        {
            return -1;
        }
    }

    /* Gebe aio_errno zurueck */
    return localHead->aio_errno;
}
