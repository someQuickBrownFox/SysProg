#ifndef AIOINIT_H
#define AIOINIT_H

#include <string.h>
#include <stdlib.h>

/* Defines fuer MSQ INIT, koennen in eigenes Headerfile raus */
/* Globaler Schluessel für Initialisierung des MSQ-Betriebsmittels */
#define SCHLUESSEL	(key_t) 38266091
#define PLEN		2048
#define ERRLEN		1

int msqid = 0;
sig_t old_USR1_Handler, old_INT_Handler;

struct msgbuf {
    long mtype;
    char mtext[PLEN];
};

struct aiocb *HeadPtr = NULL;




#endif /* end AIOINIT_H */
