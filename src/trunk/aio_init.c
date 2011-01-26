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

/*---------------------------------------------------------------------------------------------
  
  aio_init.c:
  
     - Definition der Signalbehandlung (fuer SIGUSR1)
     ================================================
       - Auslesen und ankommender Daten aus dem Botschaftskanal
       - adaequates Updaten des entsprechenden Kontrollblocks

     - Initialisierungsarbeiten
     ==========================
       - Einrichten der Signalbehandlung (SIGUSR1, SIGINT, SIGTERM)
       - Einrichten des Botschaftskanals

     - Aufraeumarbeiten (Aufruf: manuell und zusaetzlich auch via Signalbehandlung)
     ==============================================================================
       - Zuruecksetzen der jeweiligen Signalbehandlungen auf ihr urspruengliches Verhalten
       - Loeschen des Botschaftskanals


    FIXME:
    - Fehlerueberpruefung bei saemtlichen malloc-Aufrufen!
    - Ueberpruefung des Rueckgabewertes von updateCB() bei Verwendung innerhalb von sighand()
    - exit(1) im unteren Drittel von aio_cleanup wirklich sinnvoll? (--> Rueckgabewert!)
       
----------------------------------------------------------------------------------------------*/


///////////////////////////////////////////////////////////////////////////////////////////////

/* Prototypen */
int queue_stat(int);
int updateCB(struct msgbuf*, int);


/* Initialwert fuer globalen HeadPtr */
struct aiocb *HeadPtr = NULL;

///////////////////////////////////////////////////////////////////////////////////////////////

/* Signalbehandlung (wird an SIGUSR1 gebunden) */
void sighand(int sig) {

    /* Debug-Information */
	printf("aio_init.c: Signal %d erhalten!\n", sig);

	struct msgbuf buffer;	/* Puffer fuer zu empfangende Nachrichten */
	int blen;				/* Stringlaenge der empfangenen Nachricht */
	int msq_stat;			/* Queue-Status */
	int msqid;				/* Identifikator fuer Botschaftskanal */
    

	/* Botschaftskanal erzeugen bzw. Identifikator anfordern */
    if((msqid = msgget(SCHLUESSEL, IPC_CREAT | 0600)) == -1)
    {
        perror("Identifikator fuer Botschaftskanal kann nicht angefordert werden");
        exit(1);
	}

	/* Status des Botschaftskanals anfordern + anschlieszende Auswertung */
	msq_stat = queue_stat(msqid);
	
	if (msq_stat < 0)
		perror("Fehler beim Lesen des Queue-Status!");

	else if (msq_stat == 0)
		/* sollte eigentlich nicht auftreten */
		perror("Signal SIGUSR1 erhalten, jedoch MessageQueue leer!");

	else if (msq_stat > 0)
	{
        /* Es befinden sich Daten im Botschaftskanal - versuche, diese auszulesen! */
		do
		{
			/* Inhalt des temporaeren Puffers sicherheitshalber loeschen */
			memset(buffer.mtext, 0, PLEN);

			/* Lese aus Botschaftskanal */
			if ((blen = msgrcv(msqid, &buffer, PLEN, 0L, 0)) == -1)
			{
				perror("Fehler beim Lesen aus Botschaftskanal");
			}

			/* Debug-Information */
			printf ("aio_init.c: Gelesene Nachrichtenleange %d\n", blen);

			/* Korrespondierenden Kontrollblock updaten */
            /* FIXME: Rueckgabewert ueberpruefen? Reaktion, falls dieser -1 ist? */
			updateCB(&buffer, blen);

		} while (queue_stat(msqid) > 0);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////

/* Funktion fuer Update einer aiocb-Struktur */
int updateCB(struct msgbuf *buffer, int blen) {

    /* localHead referenziert zunaechst den ersten Kontrollblock */
    struct aiocb *localHead = HeadPtr;

    /* Pointerzuweisung nicht erfolgreich bzw. globaler HeadPtr ungueltig */
    if (!localHead)
    {
        return -1;
    }
     
    /* Suche passenden Kontrollblock */
    while (localHead->aio_pid != buffer->mtype)
    {
        if (!(localHead = localHead->aio_next))
        {
            /* Letzten Kontrollblock erreicht - Suche dennoch ohne Erfolg */
            return -1;
        }
    }
     
	/* Im Botschaftskanal wurden (noch weitere) Daten fuer einen bestimmten Auftrag uebermittelt -
       Das Kontrollblock-Attribut aio_errno sollte daher den Wert EINPROGRESS enthalten! */
	if (localHead->aio_errno != EINPROGRESS)
	{
        /* Sollte eigentlich nicht eintreten */
		return -1;
	}
	 
	/* Error Code aus Empfangspuffer auslesen und in aio_errno des entsprechenden Kontrollblocks setzen */
	localHead->aio_errno = buffer->mtext[0];
	 
	/* Debug-Information */
	printf ("aio_init.c: Payload Länge %d\n",  blen-ERRLEN);
	printf ("aio_init.c: Error Code: %d\n", localHead->aio_errno);
	 
	/* Fallunterscheidung: Lese- oder Schreibauftrag?
       (d.h. Wurden die aus dem Botschaftskanal empfangenen Daten infolge eines aio_read()- oder aio_write()-Aufrufs gesendet? */
	if (localHead->aio_lio_opcode == O_READ)
	{ /* Leseauftrag! Schreibe empfangene Daten in entsprechenden Kontrollblock */
        
		/* Zielbuffer gueltig? */
		if (localHead->aio_buf)
		{
			size_t oldSize = localHead->aio_nbytes;							/* ehemalige Nachrichtenlaenge notieren */
			localHead->aio_nbytes = localHead->aio_nbytes+blen-ERRLEN;		/* Aktualisiere Laengenangabe im Kontrollblock */
	 
			/* Schreiben der Nutzdaten */
			char *newBuffer = malloc(localHead->aio_nbytes+oldSize);		/* Allokiere neuen Speicher */
			memset(newBuffer, 0, sizeof(newBuffer));						/* Saeuberung des neuen Speichers (sicherheitshalber) */
			
			memcpy(newBuffer, localHead->aio_buf, oldSize);					/* Sichere ggf. bereits vorhandene Pufferinhalte in neuen Speicher */
	 
			memcpy(newBuffer+oldSize, buffer->mtext+ERRLEN,blen-ERRLEN);	/* Anhaengen der neuen Daten */
			
			free(localHead->aio_buf);										/* Gebe alten Speicher frei */
			
			localHead->aio_buf = newBuffer;									/* Pufferadresse des Kontrollblocks zeigt nun auf neuen Speicher */
	 
			/* Debug-Information */
			printf("aio_init.c: payload: -%s-\n", newBuffer);
		}
		else
		{ /* "Erstes" Schreiben, Kontrollblock hat bisher noch keine Nutzdaten beinhaltet */
            
			localHead->aio_nbytes = blen-ERRLEN;						  /* Notiere Laenge der Nutzdaten im Kontrollblock */
			char *newBuffer = malloc(localHead->aio_nbytes);			  /* Allokiere neuen Speicher */
			memset(newBuffer, 0, sizeof(newBuffer));					  /* Saeuberung des neuen Speichers (sicherheitshalber) */
			memcpy(newBuffer,  buffer->mtext+ERRLEN, blen-ERRLEN);		  /* Schreiben der neuen Daten */
			localHead->aio_buf = newBuffer;								  /* Pufferadresse des Kontrollblocks zeigt nun auf neuen Speicher */
	 
			/* Debug-Information */
			printf("aio_init.c: payload: -%s-\n", newBuffer);
		}
	}
	else if (localHead->aio_lio_opcode == O_WRITE)
	{ /* Schreibauftrag! Notiere lediglich die Anzahl der durch aio_write() geschriebener Bytes */
        
        /* Herauslesen der Anzahl von aio_write() geschriebener Bytes */
        size_t nbytes = 0;
        memcpy(&nbytes,  buffer->mtext+ERRLEN, sizeof(size_t));

		/* Notiere in localHead->aio_nbytes, wieviel Bytes geschrieben wurden */
        localHead->aio_nbytes = nbytes;
	 
        /* Debug-Information */
        //printf("aio_init.c: aio_write() hat %d bytes geschrieben\n", localHead->aio_nbytes);
	}
    else
    { /* localhead->aio_lio_opcode ist weder O_READ, noch O_WRITE - sollte nicht vorkommen! */
        return -1;
    }
	 
	/* Gebe Anzahl der aus dem Botschaftskanal gelesener bzw. durch 'aiosrv' geschriebener Nutzdatenbytes zurueck */
	return blen-ERRLEN;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

/* Liefere Information ueber aktuellen Status des Botschaftskanals */
int queue_stat(int msqid)
{
        struct msqid_ds tmp;

        if (msgctl(msqid, IPC_STAT, &tmp) == -1)
            /* Fehlerfall */
            return -1;
        
        else if (tmp.msg_qnum > 0)
            /* Nachrichten vorhanden, gebe Anzahl zurueck */
            return tmp.msg_qnum;
        
        else
            /* momentan keine Nachrichten in der Queue */
            return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

/* Programmende - manueller Aufruf;
   Aufraeumarbeiten (nicht Bestandteil der vorgegebenen API) */
int aio_cleanup()
{
    /* Debug-Information */
    printf("aio_init.c: cleanup!n");
    
    int ret = 0;
    
    /* Zurecksetzen der Signalbehandlungsroutinen */
    if((signal (SIGUSR1, old_USR1_Handler)) == SIG_ERR)
    {
        perror("Fehler beim Zuruecksetzen der Behandlungsroutine fuer SIGUSR1");
        ret = -1;
    }
    
    if((signal (SIGINT,  old_INT_Handler)) == SIG_ERR)
    {
        perror("Fehler beim Zuruecksetzen der Behandlungsroutine fuer SIGINT");
        ret = -1;
    }
    
    if((signal (SIGTERM,  old_TERM_Handler)) == SIG_ERR)
    {
        perror("Fehler beim Zuruecksetzen der Behandlungsroutine fuer SIGTERM");
        ret = -1;
    }

    /* Loeschen des Botschaftskanals */
    int msqid = msgget(SCHLUESSEL, IPC_CREAT | 0600);

    if (msqid != -1)
    {
        if (msgctl(msqid, IPC_RMID, NULL) == -1)
        {
            perror("Fehler beim Löschen des Botschaftskanals");
            ret = -1;
        }
    }
    else
    {
        perror("Identifikator fuer Botschaftskanal kann nicht angefordert werden");
        exit(1);
    }

    return ret;
}

/* Behandlungsroutine fuer SIGINT und SIGTERM */
void aio_cleanupWrapper()
{
    int exitVal;
    if ((exitVal = aio_cleanup()) == 0)
        exit(0); /* Aufraeumarbeiten erfolgreich! */
    
    exit(1); /* Fehler bei Aufraeumarbeiten */
}


/* Initialisierung
   Rueckgabewert - Erfolg: 0, Fehler: -1
*/
int aio_init()
{
    int msqid;
    
    /* Signale an Signalabfangsroutinen binden + urspruengliche Behandlung fuer spaetere Ruecksetzung notieren */
    if ((old_USR1_Handler = signal (SIGUSR1, &sighand)) == SIG_ERR)
    {
        perror("Fehler beim Binden der Behandlungsroutine fuer SIGUSR1");
        return -1;
    }

    if ((old_INT_Handler = signal (SIGINT,  &aio_cleanupWrapper)) == SIG_ERR)
    {
        signal (SIGUSR1, old_USR1_Handler);
        perror("Fehler beim Binden der Behandlungsroutine fuer SIGINT");
        return -1;
    }
    
    if ((old_TERM_Handler = signal (SIGTERM,  &aio_cleanupWrapper)) == SIG_ERR)
    {
        signal (SIGUSR1, old_USR1_Handler);
        signal (SIGINT, old_INT_Handler);
        perror("Fehler beim Binden der Behandlungsroutine fuer SIGTERM");
        return -1;
    }

    
    /* Botschaftskanal einrichten oder Identifikator anfordern */
    if ((msqid = msgget(SCHLUESSEL, IPC_CREAT|0600)) == -1 )
    {
        perror("Fehler beim Erzeugen des Botschaftskanals");

        /* Zuruecksetzen der Signalbehandlungen */
        signal (SIGUSR1, old_USR1_Handler);
        signal (SIGINT,  old_INT_Handler);
        signal (SIGTERM,  old_TERM_Handler);

        return -1;
    }

    return 0;
}
