#ifndef AIOINIT_H
#define AIOINIT_H

#include <string.h>
#include <stdlib.h>

/* Defines fuer MSQ INIT, koennen in eigenes Headerfile raus */
/* Globaler Schluessel für Initialisierung des MSQ-Betriebsmittels */
#define SCHLUESSEL	(key_t) 38266091
#define PLEN		2048
#define ERRLEN		1

/* Globale Variablen zwecks Sicherung und anschlieszender Ruecksetzung der Signalbehandlung */
sig_t old_USR1_Handler, old_INT_Handler, old_TERM_Handler;

/* Instanz des Prototyps fuer eine Nachricht des Botschaftskanals */
struct msgbuf {
    long mtype;
    char mtext[PLEN];
};

/* Globale Deklaration des Listenankers */
extern struct aiocb *HeadPtr;



#endif /* end AIOINIT_H */
