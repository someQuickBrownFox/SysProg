#ifndef TESTHEADER_H
#define TESTHEADER_H

#include <string.h>
#include <stdlib.h>

/* Defines fuer MSQ INIT, koennen in eingenes Headerfile raus */
/* Globaler Schluessel für Initialisierung des MSQ-Betriebsmittels */
#define SCHLUESSEL	(key_t) 38266091
#define PLEN		2048
#define ERRLEN		4

struct msgbuf {
    long mtype;
    char mtext[PLEN];
};

#endif /* end TESTHEADER_H */
