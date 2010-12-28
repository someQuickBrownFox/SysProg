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
	struct msgbuf {
		long mtype;
		char mtext[PLEN];
	} *buffer;

	int msqid;
	char text[PLEN];
	//struct msgbuf *buffer;
	int blen;
	pid_t vater;
	pid_t ich;
	ich = getpid();
	
	if ((msqid = msgget(SCHLUESSEL, IPC_CREAT|0600)) == -1 ) {
		kill(getppid (), SIGKILL);
		perror("Fehler beim Erzeugen des Botschaftskanals");
		exit(1);
	}
	printf("ich bin der Sohn: msqid: %d, pid: %d\n", msqid, getpid());
	strcpy(text,"ich bin der sohn, vater");
//	strcat(text,ich);
	text[PLEN-1] = 0;
	blen = strlen(text) +1;
	if((buffer = (struct msgbuf*)malloc(sizeof(long) + blen)) == NULL) 
		abbruch("Allokationsfehler", msqid, NULL, getppid());
//	buffer->mtype = 1L;
	buffer->mtype = (long)getpid();
	strncpy(buffer->mtext, text, blen);

	vater=getppid();
	kill(vater,SIGUSR1);

	printf("Vor dem Senden: %s\n", buffer->mtext);
	if(msgsnd(msqid, buffer, blen, 0) == -1) {
		abbruch("Fehler beim schreiben in Botschaftskanal", msqid, buffer, getppid());
		free(buffer);
		exit(0);	
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
