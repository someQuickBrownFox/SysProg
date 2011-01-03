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

#include "myHeader.h"

/* Argumente: targetPID Nachricht {1, 2} */ 
int main(int argc, char *argv[])
{
    struct msgbuf buffer;                                    /* zu befuellende struct --> msgsnd() */
	int msqid;                                               /* Identifikator fuer Botschaftskanal */ 
	int blen;                                                /* Stringlaenge der zu sendenden Nachricht */ 
	pid_t empfaenger = atoi(argv[1]);                        /* pid des Emfpaengerprozesses */ 
	int mySignal = (atoi(argv[3]) == 1) ? SIGUSR1 : SIGUSR2; /* SIGUSR1 oder SIGUSR2 ? */
    printf("Signal: %d\n", mySignal);


    /* Botschaftskanal einrichten oder Identifikator anfordern */
	if ((msqid = msgget(SCHLUESSEL, IPC_CREAT|0600)) == -1 ) {
		perror("Senderprozess: Fehler beim Erzeugen des Botschaftskanals");
		exit(1);
	}

    /* Senden vorbereiten */ 
	blen = strlen(argv[2]) +1;
	buffer.mtype = mySignal; /* Nachrichtentyp */ 
	//strncat(buffer.mtext, argv[2], blen);

    char getCode[3];
    strncpy(getCode, argv[2], 2); getCode[2] = '\0';
    sprintf(buffer.mtext, "%04d", atoi(getCode));

	strncat(buffer.mtext+ERRLEN, argv[2]+2, blen);



    /* Uebermittlung via Botschaftskanal */ 
	printf("Vor dem Senden: %s\n", buffer.mtext);
    
	if(msgsnd(msqid, &buffer, blen, 0) == -1) {
		printf("Fehler beim schreiben in Botschaftskanal\n");
		exit(0);	
	}

    /* "Benachrichtigung" des Empfaengers mittels SIGUSR1 bzw. SIGUSR2 */ 
    //
    kill(empfaenger, mySignal); 
}
