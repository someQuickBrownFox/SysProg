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
  aio_init.c:
  
     - Definition des Signalhandlings
     - Auslesen ankommender Daten 
---------------------------------------------------------*/

/* Prototypen */
int queue_stat(int);
int updateCB(struct msgbuf*, int);

///////////////////////////////////////////////////////////////////////////////////////////////

/* Signalbehandlung */
void sighand(int sig) {

	printf("Signal %d erhalten!\n", sig);

	struct msgbuf buffer;	   /* Puffer fuer zu empfangende Nachrichten */
	int blen;				   /* Stringlaenge der empfangenen Nachricht */
	int msq_stat;			   /* Queue-Status */


	/* Status des Botschaftskanals anfordern */
	msq_stat = queue_stat(msqid);
	
	if (msq_stat < 0)
		perror("Fehler beim Lesen des Queue-Status!\n");

	else if (msq_stat == 0)
		/* sollte eigentlich nicht auftreten */
		perror("Signal erhalten, jeodch MessageQueue leer!\n");

	else if (msq_stat > 0)
	{
		do
		{
			/* Pufferinhalt sicherheitshalber loeschen */
			memset(buffer.mtext, 0, blen);

			/* Lese aus Botschaftskanal */
			if ((blen = msgrcv(msqid, &buffer, PLEN, 0L, 0)) == -1)
			{
				perror("Fehler beim Lesen aus Botschaftskanal\n");
			}

			/* Debug-Information */
			printf ("Gelesene Nachrichtenleange %d\n", blen);

			/* Korrespondierenden Control Block updaten */
			updateCB(&buffer,blen);

		} while (queue_stat(msqid));
	}

}

///////////////////////////////////////////////////////////////////////////////////////////////

/* Funktion für Update einer AIOCB-Instanz */
int updateCB(struct msgbuf *buffer, int blen) {
    struct aiocb *localHead = HeadPtr;

    /* Pointerzuweisung nicht erfolgreich bzw. globaler HeadPtr ungueltig */
    if (!localHead)
    {
        return -1;
    }
     
    /* Suche passenden Kontrollblock */
    while (localHead->aio_pid != (pid_t)buffer->mtype)
    {
        if (!(localHead = localHead->aio_next))
        {
            return -1;
        }
    }
     
    /* Sollte eigentlich nicht eintreten;
	   Solange noch MsgQueue-Daten ankommen, sollte aio_errno eigentlich EINPROGRESS sein! */
	if (localHead->aio_errno != EINPROGRESS)
	{
		return -1;
	}
	 
	/* Error Code aus Empfangspuffer auslesen und in aio_errno des entsprechenden Kontrollblocks setzen */
	localHead->aio_errno = buffer->mtext[0];
	 
	/* Debug-Information */
	printf ("Payload Länge %d\n",  blen-ERRLEN);
	printf ("Error Code: %d\n", localHead->aio_errno);
	 
	/* Lese- oder Schreibauftrag? */
	if (localHead->aio_lio_opcode == O_READ) /* Leseauftrag! Schreibe empfangene Daten in entsprechenden Kontrollblock */
	{
		/*	Zielbuffer gueltig? */
		if (localHead->aio_buf)
		{
			int oldSize = localHead->aio_nbytes;							/* ehemalige Nachrichtenlaenge */
			localHead->aio_nbytes = localHead->aio_nbytes+blen-ERRLEN;		/* Aktualisiere Laengenangabe im Kontrollblock */
	 
			/* Schreiben der Daten */
			char *myBuffer = malloc(localHead->aio_nbytes+oldSize);			/* Allokiere neuen Speicher */
			memset(myBuffer, 0, sizeof(myBuffer));							/* Saeuberung des neuen Speichers */
			
			memcpy(myBuffer, localHead->aio_buf, oldSize);					/* Sichere ggf. bereits vorhandene Pufferinhalte */
	 
			memcpy(myBuffer+oldSize, buffer->mtext+ERRLEN,blen-ERRLEN);		/* Anhaengen der neuen Daten */
			
			free(localHead->aio_buf);										/* Gebe alten Speicher frei */
			
			localHead->aio_buf = myBuffer;									/* Pufferadresse des CB zeigt nun auf neuen Speicher */
	 
			/* Debug-Information */
			printf("payload: -%s-\n", myBuffer);
	 
	 
		}
		else /* "Erstes" Schreiben, Kontrollblock beinhaltet noch keine Nutzdaten */
		{
			localHead->aio_nbytes = blen-ERRLEN;						  /* Notiere Laengenangabe im Kontrollblock */
			char *myBuffer = malloc(localHead->aio_nbytes);				  /* Allokiere neuen Speicher */
			memset(myBuffer, 0, sizeof(myBuffer));						  /* Saeuberung des neuen Speichers */
			memcpy(myBuffer,  buffer->mtext+ERRLEN, blen-ERRLEN);		  /* Schreiben der neuen Daten */
			localHead->aio_buf = myBuffer;								  /* Pufferadresse des CB zeigt nun auf neuen Speicher */
	 
			/* Debug-Information */
			printf("payload: -%s-\n", myBuffer);
		}
	}
	else /* Schreibauftrag! Notiere Anzahl geschriebener Bytes in aio_nbytes des entsprechenden Kontrollblocks */
	{
		/* Anzahl von aiosrv geschriebener Bytes sollte sich in mtext befinden */
	 
		// der KONVENTION?? entsprechend auslesen...
		
		/* Notiere in localHead->aio_nbytes, wieviel Bytes geschrieben wurden */
	 
	}
	 
	/* Gebe Anzahl der aus dem Botschaftskanal gelesener Nutzdatenbytes zurueck */
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

/* Programmende - Aufraeumarbeiten (nicht Bestandteil der vorgegebenen API)*/
int aio_cleanup()
{
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

    /* Loeschen des Botschaftskanals */
    if (msgctl(msqid, IPC_RMID, NULL) == -1)
    {
        perror("Fehler beim Löschen des Botschaftskanals");
        ret = -1;
    }

    return ret;
}


/* Initialisierung
   Rueckgabewert - Erfolg: 0, Fehler: -1
*/
int aio_init()
{
    /* Signale an Signalabfangsroutinen binden + urspruengliche Behandlung notieren */
    if ((old_USR1_Handler = signal (SIGUSR1, &sighand)) == SIG_ERR)
    {
        perror("Fehler beim Binden der Behandlungsroutine fuer SIGUSR1");
        return -1;
    }

    if ((old_INT_Handler = signal (SIGINT,  &aio_cleanup)) == SIG_ERR)
    {
        signal (SIGUSR1, old_USR1_Handler);
        perror("Fehler beim Zuruecksetzen der Behandlungsroutine fuer SIGINT");
        return -1;
    }

    
    /* Botschaftskanal einrichten oder Identifikator anfordern */
    if ((msqid = msgget(SCHLUESSEL, IPC_CREAT|0600)) == -1 )
    {
        perror("Fehler beim Erzeugen des Botschaftskanals");

        /* Zuruecksetzen der Signalbehandlung */
        signal (SIGUSR1, old_USR1_Handler);
        signal (SIGINT,  old_INT_Handler);

        return -1;
    }

    return 0;
}
