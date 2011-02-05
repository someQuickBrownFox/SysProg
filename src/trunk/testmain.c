 #include "aio.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>


/*----------------------------------------------------------------------------------------
  testmain.c:

    DISCLAIMER:

    Test der Funktionen aio_init(), aio_error(), aio_return(), aio_cleanup().

    Bedingt durch den momentan Projektgesamtstatus werden diese Funktionen
    _AUSSCHLIESSLICH_ hinsichtlich des Zusammenspiels mit aio_read und aio_write() getestet.
    


  
     aio_init():
     ===========
       - Defintion der Signalbehandlung
       - Einrichten des Botschaftskanals
       - Initialisierung des globalen Listenankers (HeadPtr)

     Exemplarischer Schreibauftrag:
     ==============================
       - Aufruf von aio_write()
       - Daten: Zeichenkette
       - Schreibziel: Ausgabedatei

     Exemplarischer Leseauftrag:
     ==========================
       - Aufruf von aio_read()
       - Daten: Textdaten aus Eingabedatei
       - Leseziel: Nutzdatenpuffer des entsprechenden Kontrollblocks
       
     Aufraeumarbeiten:
     =================
       - Ruecksetzen der Signalbehandlung
       - Loeschen des Botschaftskanals

----------------------------------------------------------------------------------------*/

/* Lege zwei aiocb-Strukturen an */
struct aiocb cb1, cb2;

///////////////////////////////////////////////////////////////////////////////////////////

enum zustand {INIT = 0, cb1Allocated, cb2Allocated, BOTH} Allokierungen;

/* Speicherfreigabefunktion -
   speziell auf die beiden Kontrollbloecke cb1 + cb2 dieses Testprogramms zugeschnitten! */
void freeCBs(enum zustand z)
{
    switch (z)
	{
	    case INIT:
            break;
	    case cb1Allocated:
	    	free(cb1.aio_buf);
	    	break;
            
	    case cb2Allocated:
	    	free(cb2.aio_buf);
	    	break;
            
	    case (cb1Allocated | cb2Allocated):
	    	free(cb1.aio_buf);
	    	free(cb2.aio_buf);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////
 
int main(int argc, char *argv[]) {

    /* ================================
	 * INITIALISIERUNG
     * ================================*/


    printf ("testmain gestartet (pid: %d)\n", getpid());

    /* Namen der Input- und Output Dateien */
    char input_file []  = "lala.txt";
    char output_file [] = "lala_write.txt";

    /* Initialisierung (Signalbehandlung, Botschaftskanal) */
    aio_init();

    /* Initialisierung der bereits jetzt schon benoetigten Strukturkomponenten */
    cb1.aio_buf = NULL;
    cb1.aio_offset = 0;

    cb2.aio_buf = NULL;
    cb2.aio_offset = 0;

    


    /* ================================
	 * SCHREIBAUFTRAG
     * ================================*/

    /* Zu schreibender Text */
	char text[] = "Zum inzwischen 25. Mal ruft die Gönger SerNet GmbH Anwender und Entwickler zur Samba eXPerience nach Göngen.\n";

    /* Oeffne zu schreibene Datei (Dateideskriptor fd1) */
    int fd1;
	if ((fd1 = open(output_file , O_WRONLY|O_APPEND)) == -1) {
		perror ("error opening output_file\n");
        return 1;
    }
	 
    /* Spezifiziere den Dateideskriptor im Kontrollblock */
	cb1.aio_fildes = fd1;

    /* Allokiere einen Nutzdatenpuffer fuer den  Kontrollblock (Groesse entsprechend den zu schreibenden Daten) */
	if ((cb1.aio_buf = malloc(strlen(text) + 1)) == NULL)
    {
		perror ("Fehler bei Speicheranforderung fuer Nutzdatenpuffer des Kontrollblocks cb1\n");
        return 1;
    }
    Allokierungen = cb1Allocated;

    /* Kopiere die zu schreibenden Daten in den Nutzdatenpuffer des Kontrollblocks */
	if ((memcpy(cb1.aio_buf, text, strlen(text) +1)) == NULL)
    {
		perror ("Fehler beim Befuellen des Nutzdatenpuffers des Kontrollblocks cb1\n");
        freeCBs(Allokierungen);
        return 1;
    }

    /* Spezifiziere die gewuenschte Menge tatsaechlich zu schreibender Bytes */
	cb1.aio_nbytes = strlen(text);

    /* Konnte der aio_write()-Auftrag erfolgreich aufgetragen werden? */
	if (aio_write(&cb1) != -1) 
    {
        /* Warte, solange sich der Schreibauftrag noch in Bearbeitung befindet */
        while (aio_error(&cb1) == EINPROGRESS); 

        /* Ausgabe der Fehlernummer und des Rueckgabewertes */
	    printf("Rueckgabewert von aio_error()  [Schreibauftrag - cb1]: %d\n", aio_error(&cb1));
        printf("Rueckgabewert von aio_return() [Schreibauftrag - cb1]: %d\n", (int)aio_return(&cb1));
    }
    else
    {
        perror ("Der aio_write()-Auftrag konnte nicht erfolgreich aufgetragen werden");
        freeCBs(Allokierungen);
    }

    /* Schliesse Datei */
    if (close(fd1) == -1)
    {
        perror ("Fehler beim Schliessen von output_file");
        freeCBs(Allokierungen);
    }




    /* ================================
	 * LESEAUFTRAG
     * ================================*/
    
    /* Oeffne zu lesende Datei (Dateideskriptor fd2) */
    int fd2;
	if ((fd2 = open(input_file, O_RDONLY)) == -1) {
		perror ("error opening input_file\n");
        freeCBs(Allokierungen);
        return 1;
    }
    
    /* Spezifiziere den Dateideskriptor im Kontrollblock */
	cb2.aio_fildes = fd2;

    /* Ermittle Dateigroesse und spezifiziere anschliessend die gewuenschte Menge tatsaechlich zu lesender Bytes */
	struct stat st;
    stat(input_file, &st);
	cb2.aio_nbytes = st.st_size;
    
    /* Konnte der aio_read()-Auftrag erfolgreich aufgetragen werden? */
	if (aio_read(&cb2) != -1)
    {
        Allokierungen |= cb2Allocated;
        
        /* Warte, solange sich der Leseauftrag noch in Bearbeitung befindet */
	    while (aio_error(&cb2) == EINPROGRESS); 

        /* Ausgabe der Fehlernummer und des Rueckgabewertes */
	    printf("Rueckgabewert von aio_error()  [Leseauftrag - cb2]: %d\n", aio_error(&cb2));

        ssize_t retLength = aio_return(&cb2);
	    printf("Rueckgabewert von aio_return() [Leseauftrag - cb1]: %d\n", (int)retLength);

        if (retLength >= 0)
        {
            /* Zu Testzwecken handelt es sich um menschenlesbare Zeichenketten.
             * Um diese besser ausgeben zu koennen, sollten diese in ein
             * temporaeres char-Array kopiert und nullterminiert werden */

	        char buffer[retLength+1];

	        if (memset(buffer, 0, retLength+1) == NULL)
            {
                perror ("Fehler bei Speicherinitialisierung fuer Ausgabepuffer\n");
                freeCBs(Allokierungen);
                return 1;
            }

	        if (memcpy(buffer, cb2.aio_buf,retLength) == NULL)
            {
                perror ("Fehler beim Kopieren der Nutzdaten des Kontrollblocks in den Ausgabepuffer\n");
                freeCBs(Allokierungen);
                return 1;
            }

	        int laenge = strlen(cb2.aio_buf);
	        printf("CB2 - Anzahl Nutzdatenbytes: %d\n", laenge);
	        printf("CB2 - Nutzdaten: '%s'\n", buffer);
        }

    }
    else
    {
        perror ("Der aio_read()-Auftrag konnte nicht erfolgreich aufgetragen werden");
        freeCBs(Allokierungen);
    }

    /* Schliesse Datei */
    if (close(fd2) == -1)
    {
        perror ("Fehler beim Schliessen von input_file");
        freeCBs(Allokierungen);
    }




    /* ================================
	 * AUFRAEUMARBEITEN
     * ================================*/

    if (aio_cleanup() == -1)
    {
        perror ("Fehler bei Aufraemarbeiten");
    }

    freeCBs(Allokierungen);
    
    return 0;
}
