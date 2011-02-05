 #include "../aio.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
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
struct aiocb cb1, cb2, cb3;

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
void TestCancelCompleteList()
{
      /* ================================
	 * INITIALISIERUNG
     * ================================*/
    printf ("testcancel gestartet (pid: %d)\n", getpid());

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
    //cb2.aio_next = NULL;
    
    cb3.aio_buf = NULL;
    cb3.aio_offset = 0;


    /* ================================
	 * LESEAUFTRAG
     * ================================*/
    
    /* Oeffne zu lesende Datei (Dateideskriptor fd2) */
    int fd2;
    if ((fd2 = open(input_file, O_RDONLY)) == -1) 
    {
	perror ("error opening input_file\n");
        freeCBs(Allokierungen);
        return;
    }
    
    /* Spezifiziere den Dateideskriptor im Kontrollblock */
    cb2.aio_fildes = fd2;
    cb3.aio_fildes = fd2;
    /* Ermittle Dateigroesse und spezifiziere anschliessend die gewuenschte Menge tatsaechlich zu lesender Bytes */
    struct stat st;
    stat(input_file, &st);
    cb2.aio_nbytes = st.st_size;
    cb3.aio_nbytes = st.st_size;
	
    /* Konnte der aio_read()-Auftrag erfolgreich aufgetragen werden? */
    aio_read(&cb3);    
    aio_read(&cb2);
    aio_cancel(fd2,NULL);

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
    
    return;
}

void TestFildes()
{
    if(aio_cancel(-1,NULL) == -1)
    {
      printf("aio_cancel: BadFileDescriptor\n"); //debug
    }
    return;
}

void TestKillOnce()
{
     /* ================================
	 * INITIALISIERUNG
     * ================================*/
    printf ("testcancel gestartet (pid: %d)\n", getpid());

    /* Namen der Input- und Output Dateien */
    char input_file []  = "lala.txt";

    /* Initialisierung (Signalbehandlung, Botschaftskanal) */
    aio_init();

    /* Initialisierung der bereits jetzt schon benoetigten Strukturkomponenten */
    cb1.aio_buf = NULL;
    cb1.aio_offset = 0;

    /* ================================
	 * LESEAUFTRAG
     * ================================*/
    
    /* Oeffne zu lesende Datei (Dateideskriptor fd2) */
    int fd1;
    if ((fd1 = open(input_file, O_RDONLY)) == -1) 
    {
	perror ("error opening input_file\n");
        freeCBs(Allokierungen);
        return;
    }
    
    /* Spezifiziere den Dateideskriptor im Kontrollblock */
    cb1.aio_fildes = fd1;
    /* Ermittle Dateigroesse und spezifiziere anschliessend die gewuenschte Menge tatsaechlich zu lesender Bytes */
    struct stat st;
    stat(input_file, &st);
    cb1.aio_nbytes = st.st_size;
	
    /* Konnte der aio_read()-Auftrag erfolgreich aufgetragen werden? */
    aio_read(&cb1);    
    if( aio_cancel(fd1,&cb1) == AIO_CANCELED)
    {
      printf("aio_cancel return: AIO_CANCELED (process killed by signal)\n");
      if(cb1.aio_errno == ECANCELED)
	printf("CB->ERRNO: ECANCELED\n");
    }
    
    
    
    /* Schliesse Datei */
    if (close(fd1) == -1)
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
    
    return;
}

void TestAllreadyFinished()
{
       /* ================================
	 * INITIALISIERUNG
     * ================================*/
    printf ("testcancel gestartet (pid: %d)\n", getpid());

    /* Namen der Input- und Output Dateien */
    char input_file []  = "lala_short.txt";

    /* Initialisierung (Signalbehandlung, Botschaftskanal) */
    aio_init();

    /* Initialisierung der bereits jetzt schon benoetigten Strukturkomponenten */
    cb1.aio_buf = NULL;
    cb1.aio_offset = 0;

    /* ================================
	 * LESEAUFTRAG
     * ================================*/
    
    /* Oeffne zu lesende Datei (Dateideskriptor fd2) */
    int fd1;
    if ((fd1 = open(input_file, O_RDONLY)) == -1) 
    {
	perror ("error opening input_file\n");
        freeCBs(Allokierungen);
        return;
    }
    
    /* Spezifiziere den Dateideskriptor im Kontrollblock */
    cb1.aio_fildes = fd1;
    /* Ermittle Dateigroesse und spezifiziere anschliessend die gewuenschte Menge tatsaechlich zu lesender Bytes */
    struct stat st;
    stat(input_file, &st);
    cb1.aio_nbytes = st.st_size;
	
    /* Konnte der aio_read()-Auftrag erfolgreich aufgetragen werden? */
    aio_read(&cb1);   
    sleep(1);
    if( aio_cancel(fd1,&cb1) == AIO_ALLDONE)
    {
      printf("aio_cancel return: AIO_ALLDONE (process already finished)\n");  //debug
    }
    
    /* Schliesse Datei */
    if (close(fd1) == -1)
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
    
    return;
}

void TestFildesNotMatchOnce()
{
     /* ================================
	 * INITIALISIERUNG
     * ================================*/
    printf ("testcancel gestartet (pid: %d)\n", getpid());

    /* Namen der Input- und Output Dateien */
    char input_file []  = "lala.txt";
    char input_file_short [] = "lala_short.txt";

    /* Initialisierung (Signalbehandlung, Botschaftskanal) */
    aio_init();

    /* Initialisierung der bereits jetzt schon benoetigten Strukturkomponenten */
    cb1.aio_buf = NULL;
    cb1.aio_offset = 0;

    /* ================================
	 * LESEAUFTRAG
     * ================================*/
    
    /* Oeffne zu lesende Datei (Dateideskriptor fd2) */
    int fd1;
    if ((fd1 = open(input_file, O_RDONLY)) == -1) 
    {
	perror ("error opening input_file\n");
        freeCBs(Allokierungen);
        return;
    }
    
    int fd2;
    if ((fd2 = open(input_file_short, O_RDONLY)) == -1) 
    {
	perror ("error opening input_file\n");
        freeCBs(Allokierungen);
        return;
    }
    
    /* Spezifiziere den Dateideskriptor im Kontrollblock */
    cb1.aio_fildes = fd1;
    /* Ermittle Dateigroesse und spezifiziere anschliessend die gewuenschte Menge tatsaechlich zu lesender Bytes */
    struct stat st;
    stat(input_file, &st);
    cb1.aio_nbytes = st.st_size;
	
    /* Konnte der aio_read()-Auftrag erfolgreich aufgetragen werden? */
    aio_read(&cb1);   
    aio_cancel(fd2,&cb1);
    if( errno == EINVAL)
    {
      printf("aio_cancel return: Fildes Not Match\n");  //debug
    }
    
    while (aio_error(&cb1) == EINPROGRESS); 
    
    if (close(fd1) == -1)
    {
        perror ("Fehler beim Schliessen von input_file");
        freeCBs(Allokierungen);
    }
    
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
    
    return;
}

void TestAlldone()
{
    printf ("testcancel gestartet (pid: %d)\n", getpid());
    char input_file []  = "lala.txt";
    aio_init();

    cb1.aio_buf = NULL;
    cb1.aio_offset = 0;
    
    cb2.aio_buf = NULL;
    cb2.aio_offset = 0;

    int fd1;
    if ((fd1 = open(input_file, O_RDONLY)) == -1) 
    {
	perror ("error opening input_file\n");
        freeCBs(Allokierungen);
        return;
    }
    
   
    cb1.aio_fildes = fd1;
    cb2.aio_fildes = fd1;
    
    struct stat st;
    stat(input_file, &st);
    cb1.aio_nbytes = st.st_size;
    cb2.aio_nbytes = st.st_size;
    
    aio_read(&cb1);
    while (aio_error(&cb1) == EINPROGRESS); 
    aio_read(&cb2);
    while (aio_error(&cb2) == EINPROGRESS); 
    
    int back = aio_cancel(fd1,NULL);
    if(back  == AIO_ALLDONE)
    {
      printf("aio_cancel return: AIO_ALLDONE (all processes allready done)\n");
      //if(cb1.aio_errno == ECANCELED)
	//printf("CB->ERRNO: ECANCELED\n");
    }

    if (close(fd1) == -1)
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
    
    return;
}

void TestKillAll()
{
    printf ("testcancel gestartet (pid: %d)\n", getpid());
    char input_file []  = "lala.txt";
    aio_init();

    cb1.aio_buf = NULL;
    cb1.aio_offset = 0;
    
    cb2.aio_buf = NULL;
    cb2.aio_offset = 0;

    int fd1;
    if ((fd1 = open(input_file, O_RDONLY)) == -1) 
    {
	perror ("error opening input_file\n");
        freeCBs(Allokierungen);
        return;
    }
    
   
    cb1.aio_fildes = fd1;
    cb2.aio_fildes = fd1;
    
    struct stat st;
    stat(input_file, &st);
    cb1.aio_nbytes = st.st_size;
    cb2.aio_nbytes = st.st_size;
    
    aio_read(&cb1);   
    aio_read(&cb2);
    int back = aio_cancel(fd1,NULL);
    if(back  == AIO_CANCELED)
    {
      printf("aio_cancel return: AIO_CANCELED (all processes killed by signal)\n");
      //if(cb1.aio_errno == ECANCELED)
	//printf("CB->ERRNO: ECANCELED\n");
    }

    if (close(fd1) == -1)
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
    
    return;
}

void TestOnceDone()
{
    printf ("testcancel gestartet (pid: %d)\n", getpid());
    char input_file []  = "lala.txt";
    aio_init();

    cb1.aio_buf = NULL;
    cb1.aio_offset = 0;
    
    cb2.aio_buf = NULL;
    cb2.aio_offset = 0;

    int fd1;
    if ((fd1 = open(input_file, O_RDONLY)) == -1) 
    {
	perror ("error opening input_file\n");
        freeCBs(Allokierungen);
        return;
    }
    
   
    cb1.aio_fildes = fd1;
    cb2.aio_fildes = fd1;
    
    struct stat st;
    stat(input_file, &st);
    cb1.aio_nbytes = st.st_size;
    cb2.aio_nbytes = st.st_size;
    
    aio_read(&cb1);
    while (aio_error(&cb1) == EINPROGRESS); 
    aio_read(&cb2);    
    int back = aio_cancel(fd1,NULL);
    if(back  == AIO_NOTCANCELED)
    {
      printf("aio_cancel return: AIO_NOTCANCELED (at least one process allready done)\n");
      //if(cb1.aio_errno == ECANCELED)
	//printf("CB->ERRNO: ECANCELED\n");
    }

    if (close(fd1) == -1)
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
    
    return;
}

int main(int argc, char *argv[]) 
{
    //Zum Testen entsprechende Funktion einkommentieren
    //TestCancelCompleteList();
    //TestFildes();
    //TestKillOnce();
    //TestAllreadyFinished();
    //TestFildesNotMatchOnce();
    //TestKillAll();
    //TestAlldone();
    //TestOnceDone();
    return 0;
}
