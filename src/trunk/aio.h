#ifndef AIO_H
#define AIO_H

#include <sys/types.h>
#include <signal.h>
#include <sys/errno.h>

#define O_READ 0
#define O_WRITE 1
#define O_BLOCK 1

#define LIO_WAIT   0x00
#define LIO_NOWAIT 0x01

#define AIO_CANCELED            0x1
#define AIO_NOTCANCELED         0x2
#define AIO_ALLDONE             0x3


/* STRUCTS */
struct aiocb {
   int          aio_fildes;       /* Dateinummer fd */
   off_t        aio_offset;       /* Zugriffsposition */
   void *       aio_buf;          /* Datenpuffer */
   size_t       aio_nbytes;       /* Pufferlaenge/Anzahl Bytes im Puffer */
   int          aio_reqprio;      /* Auftragsprioritaet */
   struct sigevent  aio_sigevent; /* Signalspezifikation */
   int          aio_lio_opcode;   /* Opcode bei Verwendung als Listenelement, */
   pid_t        aio_pid;          /* fuer Prozessnummer des Sohnprozesses, der den Auftrag ausführt */
   int          aio_errno;        /* 0 bei erfolgreicher Ausführung des Auftrags;
                                     errno des Auftrags bei Fehler;
                                     EINPROGRESS, falls Auftrag noch nicht abgeschlossen */
   struct aiocb *aio_next;        /* Zeiger auf naechsten Auftrag fuer globale Liste */
};



/* FUNKTIONEN */
int aio_read (struct aiocb *aiocbp);

int aio_write (struct aiocb *aiocbp);

int lio_listio (int mode,              /* blockierend oder nicht */
                struct aiocb *list[],  /* Auftragsliste */
                int nent,              /* Laenge der Auftragsliste */
                struct sigevent *sig); /* Signal: alle erledigt */

int aio_cancel (int fildes, struct aiocb *aiocbp);

int aio_init();
    
int aio_cleanup();

size_t aio_return (struct aiocb *aiocbp);

int aio_error (struct aiocb *aiocbp);

int aio_suspend (struct aiocb *list[], int nent, struct timespec *timeout);



/* SIGNALHANDLING */
void sigHandler (int sig_nr); /* Akzeptiert ausschlieslich SIGUSR1*/


#endif /* end AIO_H */
