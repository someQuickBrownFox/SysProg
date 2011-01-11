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

#include "aioinit.h"

/* Argumente: targetPID Nachricht {1, 2} */ 
int main(int argc, char *argv[])
{
    struct msgbuf buffer;                                    /* zu befuellende struct --> msgsnd() */
	int msqid;                                               /* Identifikator fuer Botschaftskanal */ 
	pid_t empfaenger = atoi(argv[1]);                        /* pid des Emfpaengerprozesses */ 

       int errCode = atoi(argv[2]);

    printf("ZielCb: %d\n", atoi(argv[4]));


    /* Botschaftskanal einrichten oder Identifikator anfordern */
	if ((msqid = msgget(SCHLUESSEL, IPC_CREAT|0600)) == -1 ) {
		perror("Senderprozess: Fehler beim Erzeugen des Botschaftskanals");
		exit(1);
	}

    /* Senden vorbereiten */ 
        buffer.mtype = atoi(argv[4]); /* Nachrichtentyp */

        memset(buffer.mtext,0,sizeof(buffer.mtext));
        buffer.mtext[0] = (char)errCode;

        strncat(buffer.mtext, argv[3], strlen(argv[3]));
        buffer.mtext[strlen(buffer.mtext)] = '\0';

        int msgsize = ERRLEN+strlen(argv[3]);

    /* Uebermittlung via Botschaftskanal */ 
        printf("Leange derr Nachricht: %d \n",msgsize );
		printf("Vor dem Senden: %s\n", buffer.mtext);

        printf ("%s\n", argv[3]);


        if(msgsnd(msqid, &buffer, msgsize, 0) == -1) {
		printf("Fehler beim schreiben in Botschaftskanal\n");
		exit(0);	
	}

    /* "Benachrichtigung" des Empfaengers mittels SIGUSR1 bzw. SIGUSR2 */ 
    //
    kill(empfaenger, SIGUSR1);

    return 0;
}
