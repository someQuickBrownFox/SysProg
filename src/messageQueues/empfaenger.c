#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <string.h>

#include "testHeader.h"
#include "aio.h"
/////////////////////////////////////////////////////////////////////////////////////////////////

void writeCB(int setErrorCode, char *setBuffer, unsigned long whichType) {
    struct aiocb *localHead = HeadPtr;

    /* Suche zu getType zugehoeriges Glied */
    while (localHead && localHead->aio_pid != whichType)
        localHead = localHead->aio_next;

    /* Setzte Errorcode */
    localHead->aio_errno = setErrorCode;

    /* Setzte Nutzdaten */
    int oldSize = sizeof(localHead->aio_buf);

    char *buffer = malloc(oldSize + sizeof(setBuffer));   /* Allokiere neuen Speicher */
    memcpy(buffer, localHead->aio_buf, oldSize);          /* Sichere ggf. vorhandene Pufferinhalte */
    memcpy(buffer+oldSize, setBuffer, sizeof(setBuffer)); /* Anhaengen der neuen Daten */
        
    free(localHead->aio_buf);                             /* Gebe alten Speicher frei */
    localHead->aio_buf = buffer;                          /* aio_buf zeigt nun auf neuen Speicher */
}

/////////////////////////////////////////////////////////////////////////////////////////////////

/* '-1' im Fehlerfall, '0' Queue leer, n>0 Nachrichten vorhanden (Anzahl) */
int queue_stat(int msqid) 
{
	struct msqid_ds tmp;
	if (msgctl(msqid, IPC_STAT, &tmp) == -1)
		return -1;
	else if (tmp.msg_qnum > 0) 
		return tmp.msg_qnum;
	else 
		return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void sighand(int sig) {

	printf("Signal %d erhalten!\n", sig);
    
    /* Botschaftskanal einrichten oder Identifikator anfordern */
	int msqid;
	if ((msqid = msgget(SCHLUESSEL, IPC_CREAT|0600)) == -1 ) {
		perror("Fehler beim Erzeugen des Botschaftskanals");
		exit(1);
	}

    if (sig == SIGINT) { /* Prozessabbruch */
        if (msgctl(msqid, IPC_RMID, NULL) == -1) /* Botschaftskanal loeschen */
            perror("Fehler beim Löschen des Botschaftskanals");
        exit (1);
        
    } else { /* "Benachrichtigung" ueber neue Nachricht  */

	    struct msgbuf buffer;                /* Puffer fuer zu empfangende Nachrichten */
	    int blen;                            /* Stringlaenge der empfangenen Nachricht */
	    int msq_stat = queue_stat (msqid);   /* Queue-Status anfordern */

        char errCodeString[5];               /* Error Code */
        int errCode;                         /* Error Code */
        char payload[2042];                  /* Nachricht Nutzdaten */
	     
	    /* Botschaftskanal lesen */
	    if (msq_stat < 0)
	        perror("Fehler beim Lesen des Queue-Status!\n");
        
	    else if (msq_stat == 0) 
	        printf("MessageQueue ist leer!\n");
        
	    else if (msq_stat > 0) {
	        printf("%d Nachrichten vorhanden...Leseversuch!\n", msq_stat);
            /* Lese _alle_ Nachrichten */
            do {
                if ((blen = msgrcv(msqid, &buffer, PLEN, 0L, 0)) == -1) {
                    printf("err: Fehler beim Lesen aus Botschaftskanal\n");
                }

                /* ZWEITE SCHLEIFE fuer Typ der ersten gelesenen Nachricht? */
                
                /* Error Code lesen */
                strncpy(errCodeString, buffer.mtext, ERRLEN); errCodeString[sizeof(errCodeString)] = '\0';
                errCode = (int)atoi(errCodeString);
                printf ("Error Code: %d\n", errCode); /* --> aio_cb.ERRORCODE */


                /* Nutzdaten lesen */
                if (strlen (buffer.mtext) > ERRLEN+1) {
                    
                    strncpy(payload, buffer.mtext+ERRLEN, strlen(buffer.mtext)-ERRLEN); payload[sizeof(payload)+1] = '\0';
                    printf("%d Zeichen auf Kanal %lu empfangen: %s\n", blen, buffer.mtype, payload); /* --> aio_cb.PAYLOAD */

                    /* Schreibe errCode + payload in entsprechendes Glied der aio_cb-Liste */                    
                    writeCB(errCode, payload, buffer.mtype);

                }
            } while (queue_stat (msqid));
	    }
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {

    /* Signalhandler installieren */
	signal (SIGUSR1, &sighand);
	signal (SIGUSR2, &sighand);
	signal (SIGINT,  &sighand);

	/* Empfaengerprozess*/
	printf("Empfaengerprozess mit pid %d gestartet\n", getpid());
    while(1) {
        printf("Empfaengerprozess wartet...\n");
        sleep(1);
    }
}
