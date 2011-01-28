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
#include "aio.h"
#include "aio_util.h"
/*---------------------------------------------------------
  aio_init.c:
  
     - Definition der Signalbehandlung (fuer SIGUSR1)
     ================================================
       - Auslesen und ankommender Daten aus dem Botschaftskanal
       - adaequates Updaten des entsprechenden Kontrollblocks

     - Initialisierungsarbeiten
     ==========================
       - Einrichten der Signalbehandlung (SIGUSR1, SIGINT, SIGTERM)
       - Einrichten des Botschaftskanals

     - Aufraeumarbeiten (Aufruf: manuell und zusaetzlich auch via Signalbehandlung)
     ==============================================================================
       - Zuruecksetzen der jeweiligen Signalbehandlungen auf ihr urspruengliches Verhalten
       - Loeschen des Botschaftskanals


    FIXME:
    - DONE -- Fehlerueberpruefung bei saemtlichen malloc-Aufrufen!
    - DONE -- Entgegennahme/Ueberpruefung des Rueckgabewertes von updateCB() bei Verwendung innerhalb von sighand() --> sinnvolle Reaktion!
    - DONE -- exit(1) im unteren Drittel von aio_cleanup wirklich sinnvoll? (--> Rueckgabewert!)
    - DONE -- Signatur von updateCB() --> ssize_t nicht mehr benoetigt! --> stattdessen: lokale Variable!
    - Fehlerueberpruefung bei memcpy noetig???
       
----------------------------------------------------------------------------------------------*/


///////////////////////////////////////////////////////////////////////////////////////////////

/* Prototypen */
int queue_stat(int);
int updateCB(struct msgbuf*);


/* Initialwert fuer globalen HeadPtr */
struct aiocb *HeadPtr = NULL;

///////////////////////////////////////////////////////////////////////////////////////////////

/* Signalbehandlung (wird an SIGUSR1 gebunden) */
void sighand() {

    /* Debug-Information */
    aio_pdebug("%s (%d): Signal erhalten\n", __FILE__, __LINE__);

    struct msgbuf buffer;    /* Puffer fuer zu empfangende Nachrichten */
    ssize_t       blen;      /* Stringlaenge der empfangenen Nachricht */
    int           msq_stat;  /* Queue-Status */
    int           msqid;     /* Identifikator fuer Botschaftskanal */
    int           updateRet; /* Erfolg von updateCB() */
    

    /* Botschaftskanal erzeugen bzw. Identifikator anfordern */
    if((msqid = msgget(SCHLUESSEL, IPC_CREAT | 0600)) == -1)
    {
        aio_perror("%s (%d): Identifikator fuer Botschaftskanal kann nicht angefordert werden",__FILE__,__LINE__);
        exit(1);
    }

    /* Status des Botschaftskanals anfordern + anschlieszende Auswertung */
    msq_stat = queue_stat(msqid);
    
    if (msq_stat < 0)
        aio_perror("%s (%d): Fehler beim Lesen des Queue-Status!",__FILE__,__LINE__);

    else if (msq_stat == 0)
        /* sollte eigentlich nicht auftreten */
        aio_perror("%s (%d): Signal erhalten, jedoch MessageQueue leer!",__FILE__,__LINE__);

    else if (msq_stat > 0)
    {
        /* Es befinden sich Daten im Botschaftskanal - versuche, diese auszulesen! */
        do
        {
            /* Inhalt des temporaeren Puffers sicherheitshalber loeschen */
            memset(buffer.mtext, 0, PLEN);

            /* Lese aus Botschaftskanal */
            if ((blen = msgrcv(msqid, &buffer, sizeof(struct msgbuf), 0L, 0)) == -1)
            {
                aio_perror("%s (%d): Fehler beim Lesen aus Botschaftskanal",__FILE__,__LINE__);
                break;
            }


            // Remark: Im Idealfall sollte an dieser Stelle blen gleich sizeof(struct msgbuf) sein.

            // Wieviel Bytes sind eigentlich in mtext
            // blen = strlen(buffer.mtext+ERRLEN);
            // Diese Lösung ist eher untauglich für binäre Daten (binäre 0 mittendrin im Byte-Strom).

            // memcpy(&errno, buffer.mtext, sizeof(errno));
            // memcpy(&blen, buffer.mtext + sizeof(errno), sizeof(blen));
            // memcpy(bytes, buffer.mtext + sizeof(errno) + sizeof(blen), blen);
            // Die 3 Zeilen gelten sowohl für aio_write() als auch aio_read() Aufrufe. errno ist auch nicht mehr gleich
            // ERRLEN, sondern sizeof(errno) Bytes in mtext. Die Zeilen oben sind jetzt eher so als "Abb. ähnlich" gedacht...
            // Außer die blen-Zeile. blen ist jetzt hier halt wirklich die tatsächliche Anzahl geschriebener/gelesener Bytes,
            // die dann noch in mtext folgen (im Falle von O_READ).

            /* Korrespondierenden Kontrollblock updaten */
            if ((updateRet = updateCB(&buffer)) == -1)
            { /* Schwerwiegender Fehler innerhalb von updateCB() */

                aio_perror("%s (%d): Aufruf von updateCB() nicht erfolgreich",__FILE__,__LINE__);
                exit(1);
            }
            else
            {
                aio_pdebug("%s (%d): aio_nbytes des aktuellen Kontrollblock wurde um %d Bytes inkrementiert\n", __FILE__, __LINE__, updateRet);
            }

            if (queue_stat(msqid) > 0)
                aio_pdebug("%s (%d): Noch %d weitere Nachricht%s abzuarbeiten\n", __FILE__, __LINE__,queue_stat(msqid), (queue_stat(msqid) > 1) ? "en" : "");
            else
                aio_pdebug("%s (%d): Keine weiteren Nachrichten im Botschaftskanal\n", __FILE__, __LINE__);

        } while (queue_stat(msqid));

        
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////

/* Funktion fuer Update einer aiocb-Struktur */
int updateCB(struct msgbuf *buffer) {
    aio_pdebug("%s (%d): Aufruf von updateCB()\n", __FILE__, __LINE__);

    ssize_t blen;     /* Variable zwecks Auslesen der geschriebenen bzw. gelesenen Anzahl von Bytes */
    char *newBuffer;  /* Pointer auf neuen, zu befuellenden Nutzdatenpuffer */

    /* localHead referenziert zunaechst den ersten Kontrollblock */
    struct aiocb *localHead = HeadPtr;

    /* Pointerzuweisung nicht erfolgreich bzw. globaler HeadPtr ungueltig */
    if (!localHead)
    {
        aio_perror("%s (%d): HeadtPtr ungueltig", __FILE__,__LINE__);
        return -1;
    }
     
    /* Suche passenden Kontrollblock */
    aio_pdebug("%s (%d): Suche nach Kontrollblock mit der id %d\n", __FILE__, __LINE__,buffer->mtype);
    
    while (localHead->aio_pid != buffer->mtype)
    {
        if (!(localHead = localHead->aio_next))
        {
            aio_perror("%s (%d): Letzten Kontrollblock erreicht - Suche dennoch ohne Erfolg", __FILE__,__LINE__);
            return -1;
        }
    }
    aio_pdebug("%s (%d): Kontrollblock gefunden\n", __FILE__, __LINE__);
     
    /* Im Botschaftskanal wurden (noch weitere) Daten fuer einen bestimmten Auftrag uebermittelt -
       Das Kontrollblock-Attribut aio_errno sollte daher den Wert EINPROGRESS enthalten! */
    if (localHead->aio_errno != EINPROGRESS)
    {
        /* Sollte eigentlich nicht eintreten */
        aio_perror("%s (%d): Widerspruechlicher Wert des Elements aio_errno des Kontrollblocks", __FILE__,__LINE__);
        return -1;
    }
     
    /* Error Code aus Empfangspuffer auslesen und in aio_errno des entsprechenden Kontrollblocks setzen */
    memcpy(&(localHead->aio_errno), buffer->mtext, sizeof(errno));
    memcpy(&blen, buffer->mtext + sizeof(errno), sizeof(blen));
     
    /* Fallunterscheidung: Lese- oder Schreibauftrag?
       (d.h. Wurden die aus dem Botschaftskanal empfangenen Daten infolge eines aio_read()- oder aio_write()-Aufrufs gesendet? */
    if (localHead->aio_lio_opcode == O_READ)
    { /* Leseauftrag! Schreibe empfangene Daten in entsprechenden Kontrollblock */

        
        /* Zielbuffer gueltig? */
        //if (localHead->aio_buf)
        if (localHead->aio_buf != NULL)
        {

            aio_pdebug("%s (%d): Leseauftrag wird fortgesetzt\n",  __FILE__, __LINE__);
            
            size_t oldSize = localHead->aio_nbytes;             /* ehemalige Nachrichtenlaenge notieren */
            localHead->aio_nbytes = localHead->aio_nbytes+blen; /* Aktualisiere Laengenangabe im Kontrollblock */
     
            /* Schreiben der Daten */
            if ((newBuffer = malloc(localHead->aio_nbytes)) == NULL)
            { /* Neuer Speicher konnte nicht allokiert werden */
                
                aio_perror("%s (%d): Fehler bei Allokierung des Nutzdatenpuffer fuer Kontrollblock", __FILE__,__LINE__);
                return -1;
            }
            else
            { /* Neuer Speicher wurde erfolgreich allokiert */
                    
                memset(newBuffer, 0, sizeof(newBuffer));                                  /* Saeuberung des neuen Speichers */
                 
                memcpy(newBuffer, localHead->aio_buf, oldSize);                           /* Sichere ggf. bereits vorhandene Pufferinhalte */
                 
                memcpy(newBuffer+oldSize, buffer->mtext+sizeof(errno)+sizeof(blen),blen); /* Anhaengen der neuen Daten */
                 
                free(localHead->aio_buf);                                                 /* Gebe alten Speicher frei */
                 
                localHead->aio_buf = newBuffer;                                           /* Pufferadresse des CB zeigt nun auf neuen Speicher */
            }
        }
        else
        { /* "Erstes" Schreiben, Kontrollblock hat bisher noch keine Nutzdaten beinhaltet */


            aio_pdebug("%s (%d): Neuer Leseauftrag\n",  __FILE__, __LINE__);
            
            localHead->aio_nbytes = blen;  /* Notiere Laengenangabe im Kontrollblock */
            
            if ((newBuffer = malloc(localHead->aio_nbytes)) == NULL)
            { /* Neuer Speicher konnte nicht allokiert werden */
                
                aio_perror("%s (%d): Fehler bei Allokierung des Nutzdatenpuffer fuer Kontrollblock", __FILE__,__LINE__);
                return -1;
            }
            else
            { /* Neuer Speicher wurde erfolgreich allokiert */
                
                memset(newBuffer, 0, sizeof(newBuffer));                           /* Saeuberung des neuen Speichers */
                memcpy(newBuffer,  buffer->mtext+sizeof(errno)+sizeof(blen),blen); /* Schreiben der neuen Daten */
                localHead->aio_buf = newBuffer;                                    /* Pufferadresse des CB zeigt nun auf neuen Speicher */
                 
                /* Debug-Information */
                //aio_pdebug("%s (%d): Inhalt Mtext %s\n",  __FILE__, __LINE__, buffer->mtext+sizeof(errno)+sizeof(blen));
                //aio_pdebug("%s (%d): Kopierte Daten %s\n",  __FILE__, __LINE__, (char*)localHead->aio_buf);
            }
        }
    }
    else if (localHead->aio_lio_opcode == O_WRITE)
    { /* Schreibauftrag! Notiere lediglich die Anzahl der durch aio_write() geschriebener Bytes */
        aio_pdebug("%s (%d): Schreibauftrag\n",  __FILE__, __LINE__);
        
        /* Herauslesen der Anzahl von aio_write() geschriebener Bytes */
//        size_t nbytes = 0;
//        memcpy(&nbytes,  buffer->mtext+ERRLEN, sizeof(size_t));

        /* Notiere in localHead->aio_nbytes, wieviel Bytes geschrieben wurden */
        localHead->aio_nbytes = blen;
     
        /* Debug-Information */
        //printf("aio_init.c: aio_write() hat %d bytes geschrieben\n", localHead->aio_nbytes);
    }
    else
    { /* localhead->aio_lio_opcode ist weder O_READ, noch O_WRITE - sollte nicht vorkommen! */
        
        aio_perror("%s (%d): aio_lio_opcode des Kontrollblocks ist weder O_READ noch O_WRITE", __FILE__,__LINE__);
        return -1;
    }
     
    /* Gebe Anzahl der aus dem Botschaftskanal gelesener bzw. durch 'aiosrv' geschriebener Nutzdatenbytes zurueck */
    return blen;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

/* Liefere Information ueber aktuellen Status des Botschaftskanals */
int queue_stat(int msqid)
{
        struct msqid_ds tmp;

        if (msgctl(msqid, IPC_STAT, &tmp) == -1)
            /* Fehlerfall */
            return -1;
        
        else if (tmp.msg_qnum > 0)
            /* Nachrichten vorhanden, gebe Anzahl zurueck */
            return tmp.msg_qnum;
        
        else
            /* momentan keine Nachrichten in der Queue */
            return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

/* Programmende - manueller Aufruf;
   Aufraeumarbeiten (nicht Bestandteil der vorgegebenen API) */
int aio_cleanup()
{
    aio_pdebug("%s (%d): Cleanup\n",  __FILE__, __LINE__);
    
    int ret = 0; /* Rueckgabewert */
    
    /* Zurecksetzen der Signalbehandlungsroutinen */
    if((signal (SIGUSR1, old_USR1_Handler)) == SIG_ERR)
    {
        aio_perror("%s (%d): Fehler beim Zuruecksetzen der Behandlungsroutine fuer SIGUSR1",__FILE__,__LINE__);
        ret = -1;
    }
    
    if((signal (SIGINT,  old_INT_Handler)) == SIG_ERR)
    {
        aio_perror("%s (%d): Fehler beim Zuruecksetzen der Behandlungsroutine fuer SIGINT",__FILE__,__LINE__);
        ret = -1;
    }
    
    if((signal (SIGTERM,  old_TERM_Handler)) == SIG_ERR)
    {
        aio_perror("%s (%d): Fehler beim Zuruecksetzen der Behandlungsroutine fuer SIGTERM",__FILE__,__LINE__);
        ret = -1;
    }

    /* Loeschen des Botschaftskanals */
    int msqid = msgget(SCHLUESSEL, IPC_CREAT | 0600);

    if (msqid != -1)
    {
        if (msgctl(msqid, IPC_RMID, NULL) == -1)
        {
            aio_perror("%s (%d): Fehler beim Entfernen des Botschaftskanals",__FILE__,__LINE__);
            ret = -1;
        }
    }
    else
    {
        aio_perror("%s (%d): Identifikator fuer Botschaftskanal kann nicht angefordert werden",__FILE__,__LINE__);
        ret = -1;
    }

    return ret;
}

/* Behandlungsroutine fuer SIGINT und SIGTERM */
void aio_cleanupWrapper()
{
    int exitVal;
    if ((exitVal = aio_cleanup()) == 0)
        exit(0); /* Aufraeumarbeiten erfolgreich! */
    
    exit(1); /* Fehler bei Aufraeumarbeiten */
}


/* Initialisierung
   Rueckgabewert - Erfolg: 0, Fehler: -1
*/
int aio_init()
{
    int msqid; /* Identifikator fuer  Botschaftskanal */
    
    aio_pdebug("%s (%d): Binden der Signalbehandlungsroutinen\n", __FILE__, __LINE__);
    
    /* Signale an Signalabfangsroutinen binden + urspruengliche Behandlung fuer spaetere Ruecksetzung notieren */
    if ((old_USR1_Handler = signal (SIGUSR1, &sighand)) == SIG_ERR)
    {
        aio_perror("%s (%d): Fehler beim Binden der Behandlungsroutine fuer SIGUSR1",__FILE__,__LINE__);
        return -1;
    }

    if ((old_INT_Handler = signal (SIGINT,  &aio_cleanupWrapper)) == SIG_ERR)
    {
        signal (SIGUSR1, old_USR1_Handler);
        aio_perror("%s (%d): Fehler beim Binden der Behandlungsroutine fuer SIGINT",__FILE__,__LINE__);
        return -1;
    }
    
    if ((old_TERM_Handler = signal (SIGTERM,  &aio_cleanupWrapper)) == SIG_ERR)
    {
        signal (SIGUSR1, old_USR1_Handler);
        signal (SIGINT, old_INT_Handler);
        aio_perror("%s (%d): Fehler beim Binden der Behandlungsroutine fuer SIGTERM",__FILE__,__LINE__);
        return -1;
    }

    /* Botschaftskanal einrichten oder Identifikator anfordern */
    aio_pdebug("%s (%d): Einrichten des Botschaftskanals\n", __FILE__, __LINE__);
    
    if ((msqid = msgget(SCHLUESSEL, IPC_CREAT|0600)) == -1 )
    {
        aio_perror("%s (%d): Fehler beim Erzeugen des Botschaftskanals",__FILE__,__LINE__);

        /* Zuruecksetzen der Signalbehandlungen */
        signal (SIGUSR1, old_USR1_Handler);
        signal (SIGINT,  old_INT_Handler);
        signal (SIGTERM,  old_TERM_Handler);

        return -1;
    }
    return 0;
}
