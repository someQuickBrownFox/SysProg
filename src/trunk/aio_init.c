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

/*----------------------------------------------------------------------------------------
  aio_init.c:
  
     Definition der Signalbehandlung (fuer SIGUSR1):
     ===============================================
       - Auslesen und ankommender Daten aus dem Botschaftskanal
       - adaequates Updaten des entsprechenden Kontrollblocks

     Initialisierungsarbeiten:
     =========================
       - Einrichten der Signalbehandlung (SIGUSR1, SIGINT, SIGTERM)
       - Einrichten des Botschaftskanals

     Aufraeumarbeiten (Aufruf: manuell und zusaetzlich auch via Signalbehandlung):
     =============================================================================
       - Zuruecksetzen der jeweiligen Signalbehandlungen auf ihr urspruengliches Verhalten
       - Loeschen des Botschaftskanals


    FIXME:
    - DONE -- Fehlerueberpruefung bei saemtlichen malloc-Aufrufen!
    - DONE -- Entgegennahme/Ueberpruefung des Rueckgabewertes von updateCB() bei
              Verwendung innerhalb von sighand() --> sinnvolle Reaktion!
    - DONE -- exit(1) im unteren Drittel von aio_cleanup wirklich sinnvoll? (--> Rueckgabewert!)
    - DONE -- Signatur von updateCB() --> ssize_t nicht mehr benoetigt!
    --> stattdessen: lokale Variable!
    - Fehlerueberpruefung bei memcpy noetig???
    - versuche realloc() statt malloc() + free() ?
    - SCHLUESSEL mittels ftok() ( ? file to key) "sessionsepzifisch generieren"
    - DONE -- Zeilenbreite hinsichtlich A4-Ausdruck anpassen!
    - sighandler: exit vs. break?
    - DONE -- aio_error.c/aio_return.c mit Debug- und Error-Funktionen versehen!
    - Dateibeschreibung fuer aio_init.c, aio_error.c und aio_return.c pruefen und ggf. aendern!
    - aio_return.c: if (predecessorCB)... ueberfluessig? (return-statements!)
       
----------------------------------------------------------------------------------------*/


//////////////////////////////////////////////////////////////////////////////////////////

/* Prototypen */
int queue_stat(int);
int updateCB(struct msgbuf*);

/* Globalen HeadPtr definieren und initialisieren */
struct aiocb *HeadPtr = NULL;

//////////////////////////////////////////////////////////////////////////////////////////

/* Signalbehandlungs-Funktion (wird an SIGUSR1 gebunden) */
void sighand() {

    aio_pdebug("%s (%d): Signal erhalten\n", __FILE__, __LINE__);

    struct msgbuf buffer;    /* Puffer fuer zu empfangende Nachrichten */
    ssize_t       blen;      /* Stringlaenge der empfangenen Nachricht */
    int           msq_stat;  /* Queue-Status */
    int           msqid;     /* Identifikator fuer Botschaftskanal */
    int           updateRet; /* Variable fuer updateCB()-Auswertung */
    

    /* Botschaftskanal erzeugen bzw. Identifikator anfordern */
    if((msqid = msgget(SCHLUESSEL, IPC_CREAT | 0600)) == -1)
    {
        aio_perror("%s (%d): Fehler beim Anfordern des Identifikators fuer Botschaftskanal"
                ,__FILE__,__LINE__);

        exit(1);
    }

    /* Status des Botschaftskanals anfordern + anschliessende Auswertung */
    msq_stat = queue_stat(msqid);
    
    if (msq_stat < 0)
        aio_perror("%s (%d): Fehler beim Lesen des Queue-Status!",__FILE__,__LINE__);

    else if (msq_stat == 0)
        aio_perror("%s (%d): Signal erhalten, jedoch MessageQueue leer!",
                __FILE__,__LINE__);

    else if (msq_stat > 0)
    { /* Es befinden sich Nachrichten im Botschaftskanal - versuche, diese auszulesen! */
        
        do
        {
            /* Inhalt des temporaeren Puffers sicherheitshalber loeschen */
            if ((memset(buffer.mtext, 0, PLEN) == NULL))
            { /* Schwerwiegendes Problem mit der Laufzeitumgebung */
            	aio_perror("%s (%d): Fehler beim Initialisieren des Puffers",
            			__FILE__,__LINE__);
            	exit(1);
            }

            /* Lese Nachricht aus Botschaftskanal */
            if ((blen = msgrcv(msqid, &buffer, sizeof(struct msgbuf), 0L, 0)) == -1)
            {
                aio_perror("%s (%d): Fehler beim Lesen aus Botschaftskanal",
                        __FILE__,__LINE__);

                break;
            }

            /* Korrespondierenden Kontrollblock updaten */
            if ((updateRet = updateCB(&buffer)) == -1)
            { /* Schwerwiegender Fehler innerhalb von updateCB() */

                aio_perror("%s (%d): Aufruf von updateCB() nicht erfolgreich",
                        __FILE__,__LINE__);

                exit(1);
            }
            else
            {
                aio_pdebug("%s (%d): aio_nbytes des aktuellen Kontrollblocks "
                        "wurde um %d Bytes inkrementiert\n",
                        __FILE__, __LINE__, updateRet);
            }

            if (queue_stat(msqid) > 0)
                aio_pdebug("%s (%d): Noch %d weitere Nachricht%s abzuarbeiten\n",
                        __FILE__, __LINE__,queue_stat(msqid),
                        (queue_stat(msqid) > 1) ? "en" : "");
            else
                aio_pdebug("%s (%d): Keine weiteren Nachrichten im Botschaftskanal\n",
                        __FILE__, __LINE__);

        } while (queue_stat(msqid)); /* solange bis keine Nachrichten mehr in der Queue liegen*/
    }
}

///////////////////////////////////////////////////////////////////////////////////////////

/* Funktion fuer Update einer aiocb-Struktur aus einer Nachricht der Queue*/
int updateCB(struct msgbuf *buffer)
{
    aio_pdebug("%s (%d): Aufruf von updateCB()\n", __FILE__, __LINE__);

    ssize_t blen; /* Anzahl der von aiowrite()/aio_read() geschriebener/gelesener Bytes */
    char *newBuffer; /* Pointer auf neuen, zu befuellenden Nutzdatenpuffer */

    /* localHead referenziert zunaechst den ersten Kontrollblock */
    struct aiocb *localHead = HeadPtr;

    
    if (!localHead)
    { /* Pointerzuweisung war nicht erfolgreich bzw.
       * globaler HeadPtr ungueltig! */

        aio_perror("%s (%d): HeadtPtr ungueltig", __FILE__,__LINE__);

        return -1;
    }
     
    /* Suche passenden Kontrollblock */
    aio_pdebug("%s (%d): Suche nach Kontrollblock mit der id %d\n",
            __FILE__, __LINE__, buffer->mtype);

    while (localHead->aio_pid != buffer->mtype)
    {
        if (!(localHead = localHead->aio_next))
        {
            aio_perror("%s (%d): Letzten Kontrollblock erreicht - Suche ohne Erfolg",
                    __FILE__,__LINE__);

            return -1;
        }
    }

    aio_pdebug("%s (%d): Kontrollblock %d gefunden\n", __FILE__, __LINE__, buffer->mtype);
     
    /* Im Botschaftskanal wurden (weitere) Daten fuer einen Auftrag uebermittelt -
       Das Kontrollblock-Attribut aio_errno sollte daher den Wert EINPROGRESS enthalten! */
    if (localHead->aio_errno != EINPROGRESS)
    {
        /* Sollte eigentlich nicht eintreten, Abbruch */
        aio_perror("%s (%d): Widerspruechlicher Wert des Kontrollblockelements aio_errno",
                __FILE__,__LINE__);

        return -1;
    }
     
    /* Error Code aus Empfangspuffer auslesen und in aio_errno des Kontrollblocks setzen
       Struktur von localHead->mtext:
       | 4 Byte errno | 8 Byte Laenge der Nutzdaten | variable Nutzdaten... | */
    
    if(memcpy(&(localHead->aio_errno), buffer->mtext, sizeof(errno)) == NULL)
    { /*Schwerwiegendes Problem in der Laufzeitumgebung */
    	aio_perror("%s (%d): Fehler beim Kopieren des aio_errno-Wertes aus der Nachricht",
    			__FILE__,__LINE__);
    	exit(1);
    }
    
    /* Nachrichtenlänge aus der Queue-Nachricht speichern */
    
    if(memcpy(&blen, buffer->mtext + sizeof(errno), sizeof(blen)) == NULL)
    { /*Schwerwiegendes Problem in der Laufzeitumgebung */
    	aio_perror("%s (%d): Fehler beim Kopieren der Nachrichtenlaenge",
    			__FILE__,__LINE__);
    	exit(1);
    }
     
    /* Fallunterscheidung: Lese- oder Schreibauftrag?
       (d.h.: Wurden die aus dem Botschaftskanal empfangenen Daten infolge
       eines aio_read()- oder aio_write()-Aufrufs gesendet? */
    if (localHead->aio_lio_opcode == O_READ)
    { /* Leseauftrag! Schreibe empfangene Daten in entsprechenden Kontrollblock */

        /* Zielbuffer gueltig? */
        if (localHead->aio_buf != NULL)
        { /* aio_buf-Zeiger des Kontrollblocks zeigt bereits auf befuellten Speicher;
             --> Anhaengen von Nutzdaten! */
         
            aio_pdebug("%s (%d): Leseauftrag fuer %d wird fortgesetzt\n",
                    __FILE__, __LINE__, localHead->aio_pid);
            
            /* ehemalige Laenge der aiocb-Nutzdaten notieren */
            size_t oldSize = localHead->aio_nbytes;

            /* Aktualisiere Laengenangabe im Kontrollblock */
            localHead->aio_nbytes = localHead->aio_nbytes+blen; 
         
            /* Schreiben der Daten */
            if ((newBuffer = malloc(localHead->aio_nbytes)) == NULL)
            { /* Neuer Speicher konnte nicht allokiert werden */
                
                aio_perror("%s (%d): "
                        "Fehler bei Allokierung des Nutzdatenpuffer fuer Kontrollblock",
                        __FILE__,__LINE__);
             
                return -1;
            }
            else
            { /* Neuer Speicher wurde erfolgreich allokiert */
                    
                /* Saeuberung des neuen Speichers */
                if(memset(newBuffer, 0, sizeof(newBuffer)) == NULL)
                { /*Schwerwiegendes Problem in der Laufzeitumgebung */
                	aio_perror("%s (%d): Fehler beim Initialisieren des Nutzdatenspeichers",
                		__FILE__,__LINE__);
                		
                	return -1;
                }
         
                /* Sichere ggf. bereits vorhandene Pufferinhalte in den neuen Speicher*/
                if(memcpy(newBuffer, localHead->aio_buf, oldSize) == NULL)
                { /*Schwerwiegendes Problem in der Laufzeitumgebung */
                	aio_perror("%s (%d): Fehler beim Sichern des alten Nutzdatenspeichers",
                		__FILE__,__LINE__);
                		
                	return -1;
                }
                 
                /* Anhaengen der empfangenen Daten */
                if(memcpy(newBuffer+oldSize, buffer->mtext+sizeof(errno)+sizeof(blen),blen) 
                	== NULL)
                { /*Schwerwiegendes Problem in der Laufzeitumgebung */
                	aio_perror("%s (%d): Fehler beim Anhaengen der Paketdaten",
                		__FILE__,__LINE__);
                		
                	return -1;
                }
                 
                /* Gebe alten Speicher frei */
                free(localHead->aio_buf); /* gibt keinen Wert fuer Test zurueck */
                 
                /* Pufferadresse des CB zeigt nun auf neuen Speicher */
                localHead->aio_buf = newBuffer;
            }
        }
        
        else
        { /* Initiales Schreiben,
           * Kontrollblock hatte bisher noch keine Nutzdaten beinhaltet */
         
            aio_pdebug("%s (%d): Neuer Leseauftrag %d\n",
                    __FILE__, __LINE__, localHead->aio_pid);
            
            /* Notiere Laengenangabe der aktuellen Nachricht im Kontrollblock */
            localHead->aio_nbytes = blen;
            
            if ((newBuffer = malloc(localHead->aio_nbytes)) == NULL)
            { /* Neuer Speicher konnte nicht allokiert werden */
                
                aio_perror("%s (%d): "
                        "Fehler bei Allokierung des Nutzdatenpuffer fuer Kontrollblock",
                        __FILE__,__LINE__);

                return -1;
            }
            else
            { /* Neuer Speicher wurde erfolgreich allokiert */
                
                /* Saeuberung des neuen Speichers */
                if(memset(newBuffer, 0, sizeof(newBuffer)) == NULL)
                { /*Schwerwiegendes Problem in der Laufzeitumgebung */
                	aio_perror("%s (%d): Fehler beim Initialisieren des neuen Speichers",
                		__FILE__,__LINE__);
                		
                	return -1;
                }

                /* Schreiben der neuen Daten */
                if(memcpy(newBuffer,  buffer->mtext+sizeof(errno)+sizeof(blen),blen) == NULL)
                { /*Schwerwiegendes Problem in der Laufzeitumgebung */
                	aio_perror("%s (%d): Fehler beim Schreiben der Daten in den Kontrollblock",
                		__FILE__,__LINE__);
                		 
                	return -1;
                }

                /* Pufferadresse des CB zeigt nun auf neuen Speicher */
                localHead->aio_buf = newBuffer;
            }
        }
    }
    else if (localHead->aio_lio_opcode == O_WRITE)
    { /* Schreibauftrag,
       * notiere lediglich die Anzahl der durch aio_write() geschriebener Bytes */
        
        aio_pdebug("%s (%d): Schreibauftrag %d\n",
                __FILE__, __LINE__, localHead->aio_pid);
        
        /* Notiere in aio_nbytes des entsprechenden Kontrollblocks,
         * wieviel Bytes geschrieben wurden */
        localHead->aio_nbytes = blen;
    }
    else
    { /* localhead->aio_lio_opcode ist weder O_READ, noch O_WRITE
       * - sollte nicht vorkommen! */
        
        aio_perror("%s (%d): "
                "aio_lio_opcode des Kontrollblocks ist weder O_READ noch O_WRITE",
                __FILE__,__LINE__);

        return -1;
    }
     
    /* Gebe Anzahl der von aio_read() gelesener bzw.
     * von aio_read() geschriebener Nutzdatenbytes zurueck */
    return blen;
}

///////////////////////////////////////////////////////////////////////////////////////////

/* Liefere Information ueber aktuellen Nachrichten-Status des Botschaftskanals */
int queue_stat(int msqid)
{
        /* Anlegen eines Prototyps der Message-Queue Struktur fuer Auswertung */
        struct msqid_ds tmp;

        /* Lade Queue-Struktur entsprechend der ID in die Prototyp-Struktur */
        if (msgctl(msqid, IPC_STAT, &tmp) == -1)
            /* Fehlerfall: Abbruch*/
            return -1;
        
        else if (tmp.msg_qnum > 0)
            /* Nachrichten vorhanden, gebe Anzahl zurueck */
            return tmp.msg_qnum;
        
        else
            /* momentan keine Nachrichten in der Queue */
            return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////

/* Aufraeumarbeiten (nicht Bestandteil der vorgegebenen API)
   Manueller Aufruf am Ende von main() */
int aio_cleanup()
{
    aio_pdebug("%s (%d): Aufruf von aio_cleanup()\n",  __FILE__, __LINE__);
    
    int ret = 0; /* Rueckgabewert */
    
    /* Zurecksetzen der Signalbehandlungsroutinen */
    if((signal (SIGUSR1, old_USR1_Handler)) == SIG_ERR)
    {
        aio_perror("%s (%d): "
                "Fehler beim Zuruecksetzen der Behandlungsroutine fuer SIGUSR1",
                __FILE__,__LINE__);

        ret = -1;
    }
    
    if((signal (SIGINT,  old_INT_Handler)) == SIG_ERR)
    {
        aio_perror("%s (%d): "
                "Fehler beim Zuruecksetzen der Behandlungsroutine fuer SIGINT",
                __FILE__,__LINE__);

        ret = -1;
    }
    
    if((signal (SIGTERM,  old_TERM_Handler)) == SIG_ERR)
    {
        aio_perror("%s (%d): "
                "Fehler beim Zuruecksetzen der Behandlungsroutine fuer SIGTERM",
                __FILE__,__LINE__);

        ret = -1;
    }

    /* Loeschen des Botschaftskanals */
    int msqid = msgget(SCHLUESSEL, IPC_CREAT | 0600);

    if (msqid != -1)
    {
        if (msgctl(msqid, IPC_RMID, NULL) == -1)
        {
            aio_perror("%s (%d): Fehler beim Entfernen des Botschaftskanals",
                    __FILE__,__LINE__);

            ret = -1;
        }
    }
    else
    {
        aio_perror("%s (%d): "
                "Identifikator fuer Botschaftskanal kann nicht angefordert werden",
                __FILE__,__LINE__);

        ret = -1;
    }

    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////

/* Aufraeumarbeiten (nicht Bestandteil der vorgegebenen API)
   --> Behandlungsroutine fuer SIGINT und SIGTERM!
   ("Wrapper" fuer aio_cleanup, da die an signal() uebergebene Signalhandlungsroutine
    idealerweise den Rueckgabetyp 'void' haben sollte) */
void aio_cleanupWrapper(int sig)
{
    aio_pdebug("%s (%d): Signal %d erhalten, Eintritt in aio_cleanupWrapper()\n",
            __FILE__, __LINE__, sig);

    int exitVal;
    if ((exitVal = aio_cleanup()) == 0)
        exit(0); /* Aufraeumarbeiten erfolgreich! */
    
    exit(1); /* Fehler bei Aufraeumarbeiten */
}


///////////////////////////////////////////////////////////////////////////////////////////

/* Initialisierung (manueller Aufruf zu Beginn von main())
   Rueckgabewert - Erfolg: 0, Fehler: -1
*/
int aio_init()
{
    aio_pdebug("%s (%d): Aufruf von aio_init()\n",  __FILE__, __LINE__);

    int msqid; /* Identifikator fuer  Botschaftskanal */
    
    aio_pdebug("%s (%d): Binden der Signalbehandlungsroutinen\n", __FILE__, __LINE__);
    
    /* Signale an Signalabfangsroutinen binden +
     * urspruengliche Behandlung fuer spaetere Ruecksetzung notieren */
    if ((old_USR1_Handler = signal (SIGUSR1, &sighand)) == SIG_ERR)
    {
        aio_perror("%s (%d): Fehler beim Binden der Behandlungsroutine fuer SIGUSR1",
                __FILE__,__LINE__);

        return -1;
    }

    if ((old_INT_Handler = signal (SIGINT,  &aio_cleanupWrapper)) == SIG_ERR)
    {
        signal (SIGUSR1, old_USR1_Handler);

        aio_perror("%s (%d): Fehler beim Binden der Behandlungsroutine fuer SIGINT",
                __FILE__,__LINE__);

        return -1;
    }
    
    if ((old_TERM_Handler = signal (SIGTERM,  &aio_cleanupWrapper)) == SIG_ERR)
    {
        signal (SIGUSR1, old_USR1_Handler);
        signal (SIGINT, old_INT_Handler);

        aio_perror("%s (%d): Fehler beim Binden der Behandlungsroutine fuer SIGTERM",
                __FILE__,__LINE__);

        return -1;
    }

    aio_pdebug("%s (%d): Einrichten des Botschaftskanals\n", __FILE__, __LINE__);

    /* Botschaftskanal einrichten oder Identifikator anfordern */
    if ((msqid = msgget(SCHLUESSEL, IPC_CREAT|0600)) == -1 )
    {
        aio_perror("%s (%d): Fehler beim Erzeugen des Botschaftskanals",__FILE__,__LINE__);

        /* Zuruecksetzen der Signalbehandlungen */
        signal (SIGUSR1, old_USR1_Handler);
        signal (SIGINT,  old_INT_Handler);
        signal (SIGTERM,  old_TERM_Handler);

        return -1;
    }

    aio_pdebug("%s (%d): Botschaftskanals mit Identifikator %d erfolgreich eingerichtet\n",
            __FILE__, __LINE__, msqid);

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////
