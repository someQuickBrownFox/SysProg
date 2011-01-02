#ifndef TESTHEADER_H
#define TESTHEADER_H


#define SCHLUESSEL	(key_t) 38266091
#define PLEN		255

struct msgbuf {
    long mtype;
    char mtext[PLEN];
};

#endif /* end TESTHEADER_H */
