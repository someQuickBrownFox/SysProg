#ifndef MYHEADER_H
#define MYHEADER_H

#include <string.h>
#include <stdlib.h>

#define SCHLUESSEL	(key_t) 38266091
#define PLEN		2048
#define ERRLEN		4

struct msgbuf {
    long mtype;
    char mtext[PLEN];
};

#endif /* end MYHEADER_H */
