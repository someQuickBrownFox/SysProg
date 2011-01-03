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

/* Argumente: targetPID Nachricht {1, 2} */ 
int main(int argc, char *argv[])
{
    struct msgbuf buffer;                                    /* zu befuellende struct --> msgsnd() */
	int msqid;                                               /* Identifikator fuer Botschaftskanal */ 
	pid_t empfaenger = atoi(argv[1]);                        /* pid des Emfpaengerprozesses */ 
	int mySignal = (atoi(argv[4]) == 1) ? SIGUSR1 : SIGUSR2; /* SIGUSR1 oder SIGUSR2 ? */
    printf("Signal: %d\n", mySignal);


    /* Botschaftskanal einrichten oder Identifikator anfordern */
	if ((msqid = msgget(SCHLUESSEL, IPC_CREAT|0600)) == -1 ) {
		perror("Senderprozess: Fehler beim Erzeugen des Botschaftskanals");
		exit(1);
	}

    /* Senden vorbereiten */ 
	buffer.mtype = mySignal; /* Nachrichtentyp */ 

    sprintf(buffer.mtext, "%04d", atoi(argv[2]));

	strncat(buffer.mtext+ERRLEN, argv[3], strlen(argv[3])+1);



    /* Uebermittlung via Botschaftskanal */ 
	printf("Vor dem Senden: %s\n", buffer.mtext);
    
	if(msgsnd(msqid, &buffer, strlen(argv[3])+4, 0) == -1) {
		printf("Fehler beim schreiben in Botschaftskanal\n");
		exit(0);	
	}

    /* "Benachrichtigung" des Empfaengers mittels SIGUSR1 bzw. SIGUSR2 */ 
    //
    kill(empfaenger, mySignal);

    return 0;
}
