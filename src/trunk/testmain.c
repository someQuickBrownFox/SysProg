#include "aio.h"
#include <stdio.h>
/* open */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
/* close */
#include <unistd.h>
#include <string.h>
#include <stdlib.h>


int main(int argc, char *argv[]) {

    printf ("foo gestartet: %d\n", getpid());

    /* Signalhandler installieren */
    aio_init();

    /* Lege zwei aiocb structs an */
    struct aiocb cb1, cb2;

    /* Initialisierung? */
    cb1.aio_buf = NULL;
    cb1.aio_offset = 0;

    cb2.aio_buf = NULL;
    cb2.aio_offset = 0;

    int fd1,fd2;
    // int mode = 1; // schreiben
    // int mode = 1; // lesen

	struct stat st;
        stat("./lala.txt", &st);
        if ((fd2 = open("./lala.txt", O_RDONLY)) == -1)
            printf("error opening lala.txt\n");
        if ((fd1 = open("lala_write.txt", O_WRONLY|O_APPEND)) == -1)
            printf("error opening lala.txt\n");
         
        cb1.aio_fildes = fd1;
         
        char text[] = "Zum inzwischen 17. Mal ruft die Gönger SerNet GmbH Anwender und Entwickler zur Samba eXPerience nach Göngen.\n";
        cb1.aio_buf = malloc(strlen(text) + 1);
        memcpy(cb1.aio_buf, text, strlen(text) +1);
        cb1.aio_nbytes = strlen(text);
        aio_write(&cb1);
         
         
	
        cb2.aio_fildes = fd2;
        cb2.aio_nbytes = st.st_size;
        printf ("aio_read returns %d\n",aio_read(&cb2));
        if (errno != 0) {
            perror("aio_read error");
        }

        while (aio_error(&cb2) == EINPROGRESS); 
        printf("errno value CB2: %d\n", aio_error(&cb2));
        while (aio_error(&cb1) == EINPROGRESS); 
        printf("errno value CB1: %d\n", aio_error(&cb1));

        printf ("sollte fertig sein...\n");
        int copy = (int)aio_return(&cb2);
        printf("return value: %d\n", copy);

        
        char buffer[copy+1];
        memset(buffer,'\0',copy+1);
        memcpy(buffer, cb2.aio_buf,copy);
        int blub = strlen(buffer);
        printf("CB2 Buffer size: %d\n", blub);
        printf("CB2 contents: '%s'\n", buffer);


    close(fd1);
    aio_cleanup();
    return 0;
}
