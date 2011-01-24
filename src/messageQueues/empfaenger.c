#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <string.h>


#include "aio.h"
#include "aio_error.c"
#include "aio_init.c"
#include "aio_return.c"
/////////////////////////////////////////////////////////////////////////////////////////////////


int main(int argc, char *argv[]) {

    /* Signalhandler installieren */
    aio_init();

    /* Lege zwei aiocb structs an (+ Verkettung)*/
    struct aiocb cb1, cb2;
    HeadPtr = &cb1;
    cb1.aio_next = &cb2;

    /* Nachrichtnetyp (=~ pid) */
    cb1.aio_pid = 10;
    cb1.aio_lio_opcode = O_READ;
    cb1.aio_buf = NULL;
    cb1.aio_errno = EINPROGRESS;
    
    cb2.aio_pid = 12;
    cb2.aio_lio_opcode = O_READ;
    cb2.aio_buf = NULL;
    cb2.aio_errno = EINPROGRESS;

    cb2.aio_next = NULL;
        

    int cb1done = 0;
    int cb2done = 0;

	/* Epfaengerprozess*/
	printf("Empfaengerprozess mit pid %d gestartet\n", getpid());
    while((aio_error(&cb1) == EINPROGRESS) || (aio_error(&cb2) == EINPROGRESS))
    {
        printf("Empfaengerprozess wartet...\n");
        sleep(1);

        if ((aio_error(&cb1) != EINPROGRESS) && cb1done == 0) {
            printf("\nAuftrag fuer CB1 abgeschlossen!\n\n");
            cb1done = 1;

            if (aio_error(&cb1) == -1 )
            {
                printf("Fehler bei Verarbeitung CB1: %d\n",aio_error(&cb1));
            }
            else
            {
                size_t len = aio_return(&cb1);
                printf ("Auftrag-CB1 %d: %s - size %d\n", cb1.aio_pid, (char*)cb1.aio_buf, (int)len);
            }
        }

        if ((aio_error(&cb2) != EINPROGRESS) && cb2done == 0) {
            printf("\nAuftrag fuer CB2 abgeschlossen!\n\n");
            cb2done = 1;

            if (aio_error(&cb2) == -1 )
            {
                printf("Fehler bei Verarbeitung CB2: %d\n",aio_error(&cb2));
            }
            else
            {
                size_t len = aio_return(&cb2);
                printf ("Auftrag-CB2 %d: %s - size %d\n", cb2.aio_pid, (char*)cb2.aio_buf, (int)len);
            }
        }
    }


    printf("\n\n\nErgebnisse:\n");
    if (aio_return(&cb1) != -1)
        printf ("Auftrag-CB1 %d: %s - size %d\n", cb1.aio_pid, (char*)cb1.aio_buf, (int)aio_return(&cb1));

    if (aio_return(&cb2) != -1)
        printf ("Auftrag-CB2 %d: %s - size %d\n", cb2.aio_pid, (char*)cb2.aio_buf, (int)aio_return(&cb2));


    aio_cleanup();

    exit(1);
}
