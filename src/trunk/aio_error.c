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
#include "aio_util.h"

/*--------------------------------------------------------------------------------------------------
  aio_error.c:
  
     Ermittlung des Abschluß- bzw. Fehlerstatus eines aio_read() bzw. aio_write()-Aufrufs:
     =====================================================================================
       --> EINPROGRESS, falls sich der Auftrag noch in Bearbeitung befindet,
       --> 0, falls der Auftrag mit Erfolg abgeschlossen wurde,
       --> "Weitergabe" von errno der zugrundeliegenden Operation,
           falls Auftrag fehlerhaft abgeschlossen;
       --> -1, falls Fehler bei aio_error() selbst.

--------------------------------------------------------------------------------------------------*/

int aio_error (struct aiocb *aiocbp)
{
    /* Deklariere und Initialisiere lokalen Anker fuer Untersuchung der linearen Liste */
    struct aiocb *localHead = HeadPtr;

    if (!localHead)
    { /* Pointerzuweisung war nicht erfolgreich bzw.
       * globaler HeadPtr ungueltig! */

        aio_perror("%s (%d): HeadtPtr ungueltig", __FILE__,__LINE__);

        return -1;
    }

    /* Suche passenden Kontrollblock in linearer aiocb-Liste */
    /* DEBUG Message auskommentiert, flutet sonst stdout
       aio_pdebug("%s (%d): Suche nach Kontrollblock mit der id %d\n",
            __FILE__, __LINE__,aiocbp->aio_pid);
    */

	/* Solange lokaler Anker nicht der verlangten aiocb-Struktur entspricht... */
    while (localHead->aio_pid != aiocbp->aio_pid)
    {
        /* Schalte solange zum nächsten Element weiter bis Ende (=NULL) erreicht */
        if (!(localHead = localHead->aio_next))
        {
            aio_perror("%s (%d): Letzten Kontrollblock erreicht - Suche ohne Erfolg",
                    __FILE__,__LINE__);

            return -1;
        }
    }

    /* DEBUG Message auskommentiert, flutet sonst stdout
    aio_pdebug("%s (%d): Kontrollblock %d gefunden\n",
            __FILE__, __LINE__, aiocbp->aio_pid);
    */

    /* Gebe aio_errno des in der Liste gefundenen Kontrollblocks zurueck */
    return localHead->aio_errno;
}
