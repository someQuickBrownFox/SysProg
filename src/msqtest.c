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

#define SCHLUESSEL	(key_t) 38266090
#define PLEN		255

/*0000000000000000000000000000000000000000000000000000000000000000000000000000*/


void sighand() 
{
	int msqid;
	if ((msqid = msgget(SCHLUESSEL, IPC_CREAT|0600)) == -1 ) {
		perror("Fehler beim Erzeugen des Botschaftskanals");
		exit(1);
	}

	signal(SIGUSR1,&sighand); 
	
	struct msgbuf {
		long mtype;
		char mtext[PLEN];
	} lesepuffer[sizeof(long) + PLEN];
	
	int msq_stat;
	int blen;
	
	long sohn;
	sohn = (long) getpid() + 2;	
	printf("singal sigusr1 erhalten!\n");
	
	/* msq lesen */
	if ((msq_stat = queue_filled(msqid)) < 0)
		perror("Fehler beim Lesen des Queue-Status\n");
	else if (msq_stat = 0) 
		printf("no message to read\n");
	else {
		printf("theres a message, lets try...\n");
		if ((blen = msgrcv(msqid, lesepuffer, PLEN, sohn, 0)) == -1) {
			abbruch("Fehler beim lesen aus Botschaftskanal", msqid, NULL, 0);
		}
		printf("Der Sohn sagte mir mit %d Zeichen: %s\n", blen, lesepuffer->mtext);
	}
}


void abbruch (char *meldung, int msqid, struct msgbuf *puffer, pid_t pid)
{
	printf("Abbruchfunktion\n");
	struct msqid_ds lpuffer;
	perror(meldung);

	kill(pid, SIGKILL);
	if (msgctl(msqid, IPC_RMID, &lpuffer) == -1) 
		perror("Fehler beim Löschen des Botschaftskanals");
	free(puffer);
	exit(1);
}

int main(int argc, char *argv[])
{
	signal (SIGUSR1,&sighand);

	

	pid_t soehne[10];
	pid_t sohn;
	//int msqid;
	struct msqid_ds lpuffer;

	int i;
	for (i = 0; i < 4; i++) {
		if ((soehne[i] = fork())  == 0) {
			/* exec child */
			execl("./child.bin",NULL);
			perror("exec() child");
		} else if (soehne[i] < 0) { /*Fehler Sohnprozess*/
			perror("nach fork()\n");
			printf("fehler %d\n",sohn);
			exit(1);
		} 
	}

	/*Vaterprozess*/
	printf("\n\nich bin der vater\n");
	printf("ich habe söhne: \n");
	int c;
	for (c = 0; c < i; c++) {
		printf("pid: %d\n", soehne[c]);
	}
}

int queue_filled(int msqid) 
{
	struct msqid_ds puffer;
	if (msgctl(msqid, IPC_STAT, &puffer) == -1)
		return -1;
	else if (puffer.msg_qnum > 0) 
		return 1;
	else 
		return 0;
} 

