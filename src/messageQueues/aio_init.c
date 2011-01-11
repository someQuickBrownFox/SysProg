#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <signal.h>
#include <string.h>

#include "aioinit.h"
///////////////////////////////////////////////////////////

void sighand(int sig) {

        printf("Signal %d erhalten!\n", sig);

     /* "Benachrichtigung" ueber neue Nachricht  */

            struct msgbuf buffer;                /* Puffer fuer zu empfangende Nachrichten */
            int blen;                            /* Stringlaenge der empfangenen Nachricht */
            int msq_stat = queue_stat(msqid);   /* Queue-Status anfordern */


            /* Botschaftskanal lesen */
            if (msq_stat < 0)
                perror("Fehler beim Lesen des Queue-Status!\n");

            /*else if (msq_stat == 0)
                printf("MessageQueue ist leer!\n");*/

            else if (msq_stat > 0)
            {
                //printf("%d Nachrichten vorhanden...Leseversuch!\n", msq_stat);
                /* Lese _alle_ Nachrichten */
                do
                {

                    if ((blen = msgrcv(msqid, &buffer, PLEN, 0L, 0)) == -1)
                    {
                        printf("err: Fehler beim Lesen aus Botschaftskanal\n");
                    }
                    printf ("Gelesene Nachrichtenleange %d\n", blen);
                    
                    updateCB(&buffer,blen);

                    memset(buffer.mtext, 0, blen);

                } while (queue_stat(msqid));
            }

}

///////////////////////////////////////////////////////////////////////////////////////////////


int updateCB(struct msgbuf *buffer, int blen) {
    struct aiocb *localHead = HeadPtr;

    /* Suche zu getType zugehoeriges Glied */
    if (!localHead) { return -1;}
    while (localHead->aio_pid != buffer->mtype) {

        if ((localHead = localHead->aio_next))
        {}
        else
        {
            return -1;
        }
    }


    /* Error Code lesen und setzen */
    memcpy(&(localHead->aio_errno), buffer->mtext, ERRLEN);
    printf ("Payload Länge %d\n",  blen-ERRLEN);
    printf ("Error Code: %d\n", localHead->aio_errno); /* --> aio_cb.ERRORCODE */


    /* Setzte Nutzdaten (kein Inhalt, falls aio_write()) */
    if (blen > ERRLEN)
    {


        if (localHead->aio_buf)
        {

            localHead->aio_nbytes = localHead->aio_nbytes+blen-ERRLEN;                        /* Notiere Laengenangabe fuer hinzugekommene bytes */

            int oldSize = strlen(localHead->aio_buf);             /* bisherige Nachrichtenlaenge (alternativ: aio_nbytes) */

            char *myBuffer = malloc(localHead->aio_nbytes+oldSize);   /* Allokiere neuen Speicher */
            memcpy(myBuffer, localHead->aio_buf, oldSize);          /* Sichere ggf. vorhandene Pufferinhalte */

            

            memcpy(myBuffer+oldSize, buffer->mtext+ERRLEN+1,blen-ERRLEN); /* Anhaengen der neuen Daten */
            //printf("eig: %s\n", buffer->mtext+ERRLEN+1);
            //printf("bla: %s\n", myBuffer);

            free(localHead->aio_buf);                             /* Gebe alten Speicher frei */
            localHead->aio_buf = myBuffer;                          /* aio_buf zeigt nun auf neuen Speicher */
            //printf ("neuer Buffer: %s\n", (char*)localHead->aio_buf);
        }
        else
        {
            localHead->aio_nbytes = blen-ERRLEN;                        /* Notiere Laengenangabe fuer hinzugekommene bytes */
            char *myBuffer = malloc(localHead->aio_nbytes);             /* Allokiere neuen Speicher */
            memcpy(myBuffer,  buffer->mtext+ERRLEN+2, blen-ERRLEN);         /* Anhaengen der neuen Daten */
            printf("eig: -%s-\n", buffer->mtext+ERRLEN+1);
            printf("bla: -%s-\n", myBuffer);
            localHead->aio_buf = myBuffer;                          /* aio_buf zeigt nun auf neuen Speicher */
        }
    }
    return blen-ERRLEN;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

/* '-1' im Fehlerfall, '0' Queue leer, n>0 Nachrichten vorhanden (Anzahl) */
int queue_stat(int msqid)
{
        struct msqid_ds tmp;
        if (msgctl(msqid, IPC_STAT, &tmp) == -1)
                return -1;
        else if (tmp.msg_qnum > 0)
                return tmp.msg_qnum;
        else
                return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////


void aio_cleanup()
{
    if (msgctl(msqid, IPC_RMID, NULL) == -1)
        perror("Fehler beim Löschen des Botschaftskanals");
    exit (1);
}

void aio_init()
{
    signal (SIGUSR1, &sighand);
    signal (SIGINT, &aio_cleanup);
    /* Botschaftskanal einrichten oder Identifikator anfordern */

    if ((msqid = msgget(SCHLUESSEL, IPC_CREAT|0600)) == -1 )
    {
        perror("Fehler beim Erzeugen des Botschaftskanals");
        exit(1);
    }
}


