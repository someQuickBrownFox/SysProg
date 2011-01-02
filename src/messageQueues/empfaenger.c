//#include "aio.h"
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
/////////////////////////////////////////////////////////////////////////////////////////////////

int queue_filled(int msqid) 
{
	struct msqid_ds tmp;
	if (msgctl(msqid, IPC_STAT, &tmp) == -1)
		return -1;
	else if (tmp.msg_qnum > 0) 
		return 1;
	else 
		return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
//
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
	    int msq_stat = queue_filled (msqid); /* Queue-Status anfordern */
	     
	    /* Botschaftskanal lesen */
	    if (msq_stat < 0)
	        perror("Fehler beim Lesen des Queue-Status!\n");
	    else if (msq_stat == 0) 
	        printf("MessageQeue ist leer!\n");
	    else if (msq_stat == 1) {
	        printf("Nachricht vorhanden...Leseversuch!\n");
	        if ((blen = msgrcv(msqid, &buffer, PLEN, sig, 0)) == -1) {
	        	printf("err: Fehler beim Lesen aus Botschaftskanal\n");
	        }
	        printf("%d Zeichen auf Kanal %d empfangen: %s\n", blen, sig, buffer.mtext);
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

    return 0;
}
