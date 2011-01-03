#ifndef AIO_H
#define AIO_H
#define EINPROGRESS 1 /* Auftrag noch nicht abgeschlossen */




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

struct aiocb *HeadPtr;

/* bereits definiert */
//struct sigevent { /* nur fuer Echtzeit-Signale */
//   int           sigev_notify; /* = SIGEV_NONE oder SIGEV_SIGNAL */
//   int           sigev_signo;  /* Signalnummer */
//   union sigval  sigev_value;  /* Signalwert */
//};


/* FUNKTIONEN */
int aio_read (struct aiocb *aiocbp);

int aio_write (struct aiocb *aiocbp);

int lio_listio (int mode,              /* blockierend oder nicht */
                struct aiocb *list[],  /* Auftragsliste/-tabelle */
                int nent,              /* Laenge der Auftragsliste */
                struct sigevent *sig); /* Signal: alle erledigt */

int aio_cancel (int fildes, struct aiocb *aiocbp);

size_t aio_return (struct aiocb *aiocbp);

int aio_error (struct aiocb *aiocbp);

int aio_suspend (struct aiocb *list[], int nent, struct timeval *timeout);


/* SIGNALHANDLING */
void sigHandler (int sig_nr); /* Akzeptiert ausschlieslich SIGUSR1*/

#endif /* end AIO_H */
