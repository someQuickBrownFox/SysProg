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
  aio_return.c:
  
     - Ermittlung des Rueckgabewertes (Aufruf im main()):
     ====================================================
       -1 im Fehlerfall, sonst:
       Anzahl gelesener bzw. geschriebener Bytes

     - Anschliessendes Aushaengen des entsprechenden Kontrollblocks aus der globalen Liste
     =====================================================================================

--------------------------------------------------------------------------------------------------*/

ssize_t aio_return (struct aiocb *aiocbp)
{
    /* Deklariere und Initialisiere lokalen Anker fuer Listendurchlauf */
    struct aiocb *localHead = HeadPtr;
    
    /* Deklariere und Initialisiere eine lokale Iterationsvariabe fuer Durchlauf und
       Bearbeitung der aiocb-Liste */ 
    struct aiocb *predecessorCB = NULL;
    
    /* Deklariere und Initialisiere den Rueckgabewert */
    int retval = -1;

	/* Fehlerbehandlung */
    if (!localHead)
    { /* Pointerzuweisung war nicht erfolgreich bzw.
       * globaler HeadPtr ungueltig! */

        aio_perror("%s (%d): HeadtPtr ungueltig", __FILE__,__LINE__);
		
		/* Setze globale Fehlervariable auf EINVAL - invalid argument */
        errno = EINVAL;
        
        /* Kehre mit Fehlerwert zurueck */
        return retval;
    }
    
    aio_pdebug("%s (%d): Suche nach Kontrollblock mit der id %d\n",
            __FILE__, __LINE__, aiocbp->aio_pid);

    /* Durchsuchen der Liste: Ist der gesuchte Kontrollblock schon das erste Listenelement? */
    if (localHead->aio_pid == aiocbp->aio_pid)
    {
        aio_pdebug("%s (%d): Kontrollblock %d gefunden\n",
                __FILE__, __LINE__, aiocbp->aio_pid);

        /* Notiere Laenge des Nutzdatenfeldes */
        retval = localHead->aio_nbytes;

        /* War der urspruengliche aio_read()- bzw. aio_write()-Aufruf erfolgreich? */
        if (retval == -1)
        {
            /* Falls nicht, wieso?
            	--> Extraktion der korrespondieren errno aus dem Kontrollblock */
            errno = localHead->aio_errno;
        }

        /* Aushaengen des gefundenen Kontrollblocks bzw. Anpassung des globalen HeadPtr -
           hat Liste insgesamt mehr als ein Element? */
        aio_pdebug("%s (%d): Aushaengen des Kontrollblocks %d aus der Liste%d\n",
                __FILE__, __LINE__, localHead->aio_pid);

        /* Wenn die Liste weitere Elemente beinhaltet... */
        if (aiocbp->aio_next)
        {
            /* ...dann wird der Anker nun um ein Element nach vorne geschoben */
            HeadPtr = aiocbp->aio_next;
        }
        else
        { 
            /* ...ansonsten aushaengen des ersten und somit einzigen Listenelementes
               damit Liste im Initialzustand! */  
            HeadPtr = NULL;
        }
        
        /* Kehre mit Laenge des Nutzdatenfeldes oder Fehlerwert zurueck */
        return retval;
    }
        
    /* Gesuchtes Glied ist nicht erstes Element, durchsuche restliche Liste */
    /* Solange localHead einen gueltigen Pointer enthaelt, aber nicht auf das richtige
       Ziel zeigt... */
    while (localHead && (localHead->aio_pid != aiocbp->aio_pid))
    {
        /* Notiere derzeitigen Pointer in Sicherungsvariable */
        predecessorCB = localHead;

        /* Pruefe ob bei der Suche das Ende der Liste erreicht wurde */
        if (!(localHead = localHead->aio_next))
        {
            aio_perror("%s (%d): Letzten Kontrollblock erreicht - Suche ohne Erfolg",
                    __FILE__,__LINE__);

            /* Setze globale Fehlervariable auf EINVAL - invalid argument */
            errno = EINVAL;
            
            /* Kehre mit Fehlerwert zurueck */
            return retval;
        }
    }

	aio_pdebug("%s (%d): Entsprechender Kontrollblock%d wurde in der Liste gefunden\n",
                __FILE__, __LINE__, localHead->aio_pid);

    /* Es wurde sicher ein korrespondierender Kontrollblock in der Liste gefunden */
    
    aio_pdebug("%s (%d): Aushaengen des Kontrollblocks aus der Liste%d\n",
                __FILE__, __LINE__, localHead->aio_pid);

    /* Aushaengen des entsprechenden Kontrollblocks aus der Liste */
    predecessorCB->aio_next = localHead->aio_next;
    
    /* Notiere Laenge des Nutzdatenfeldes im Rueckgabewert */
    retval = localHead->aio_nbytes;
        
    /* War der urspruengliche aio_read()- bzw. aio_write-Aufruf erfolgreich? */
    if (retval == -1)
    {
        /* Falls nicht, wieso?
            	--> Extraktion der korrespondieren errno aus dem Kontrollblock */
        errno = localHead->aio_errno;
    }

	/* Kehre mit Laenge des Nutzdatenfeldes oder Fehlerwert zurueck */
    return retval;
}
