#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <sys/unistd.h>
#include <sys/msg.h>

#include <fcntl.h>
#include "aio.h"
#include "aioinit.h"

#define WSCHLUESSEL (key_t) 38266095

void aiosrv_sighandler(/*int signo*/)
{
#ifdef DEBUG
    printf("Terminating...\n");
#endif
    close(0);
    exit(0);
}

void aiosrv_sighandler2()
{
}

int aiosrv_read(pid_t ppid[], int ppidlen, struct aiocb * aiocbp)
{
    char         * buffer;
    struct msgbuf  mymsgbuf;
    size_t         anz, i, j;
    int            msqid;
    int            k;

    //printf("Ich will aus DateiFd: %d lesen\n", aiocbp->aio_fildes);
    //printf("MyPID: %d\n", aiocbp->aio_pid);
    if ((buffer = (char *) malloc((aiocbp->aio_nbytes + 1) * sizeof(char))) == NULL)
    {
        return -1;
    }

    if ((msqid = msgget(SCHLUESSEL, IPC_CREAT | 0600)) == -1)
    {
        return -1;
    }
    //printf("msqid=%d\n", msqid);

    if (lseek(aiocbp->aio_fildes, aiocbp->aio_offset, SEEK_SET) == (off_t)-1)
    {
        free(buffer);
        mymsgbuf.mtype = getpid();
        mymsgbuf.mtext[0] = errno;
        if (msgsnd(msqid, &mymsgbuf, sizeof(struct msgbuf), 0) == -1)
        {
            return -1;
        }

        for (k = 0; k < ppidlen; k ++)
        {
            kill(ppid[k], SIGUSR1);
        }
        return 0;
    }

    errno = 0;
    if ((aiocbp->aio_nbytes = read(aiocbp->aio_fildes, buffer, aiocbp->aio_nbytes)) == (size_t)-1)
    {
        //printf("sizeof(buffer) = %d\n", aiocbp->aio_nbytes);
        mymsgbuf.mtype = getpid();
        mymsgbuf.mtext[0] = errno;
        msgsnd(msqid, &mymsgbuf, sizeof(struct msgbuf), 0);
        for (k = 0; k < ppidlen; k ++)
        {
            kill(ppid[k], SIGUSR1);
        }
    }
    else
    {
        anz = (int)(aiocbp->aio_nbytes / PLEN + 1);

        for (i = 0; i < anz; i++)
        {
            mymsgbuf.mtext[0] = i == anz - 1 ? 0 : EINPROGRESS;
            for (j = 1; j < PLEN && i * PLEN + j < aiocbp->aio_nbytes; j++)
            {
                mymsgbuf.mtext[j] = buffer[i * PLEN + j - 1];
            }
            msgsnd(msqid, &mymsgbuf, sizeof(struct msgbuf), 0);
            if (kill(ppid[0], SIGUSR1) == -1)
            {
                printf("killing %d failed\n", ppid[0]);
            }
            else
            {
                printf("killing %d succeeded\n", ppid[0]);
            }
            //printf("Bytes really sent: %d\n", j);
        }

        for (k = 1; k < ppidlen; k ++)
        {
            kill(ppid[k], SIGUSR1);
        }
        //printf("aiosrv: done\n");
    }
    free(buffer);
    return 0;
}

int aiosrv_write(pid_t ppid[], int ppidlen, struct aiocb * aiocbp)
{
    struct msgbuf  mymsgbuf;
    size_t         i;
    int            msqid;
    int            k;
    ssize_t        bwritten;

    //printf("Ich will in DateiFD: %d schreiben\n", aiocbp->aio_fildes);
    //printf("MyPID: %d\n", aiocbp->aio_pid);
    
    if ((msqid = msgget(SCHLUESSEL, IPC_CREAT | 0600)) == -1)
    {
        return -1;
    }

    lseek(aiocbp->aio_fildes, aiocbp->aio_offset, SEEK_SET);
    errno = 0;
    bwritten = write(aiocbp->aio_fildes, aiocbp->aio_buf, aiocbp->aio_nbytes);
    mymsgbuf.mtext[0] = errno;
    for (i = 0; i < sizeof(ssize_t); i ++)
    {
        mymsgbuf.mtext[i + 1] = ((char*) &bwritten)[i];
    }
    mymsgbuf.mtext[i + 1] = '\0';
    mymsgbuf.mtype = getpid();
    msgsnd(msqid, &mymsgbuf, sizeof(struct msgbuf), 0);
    for (k = 0; k < ppidlen; k ++)
    {
#ifdef DEBUG
        printf("killing %d: ", ppid[k]);
        if (kill(ppid[k], SIGUSR1) == -1)
        {
            printf("error\n");
        }
        else
        {
            printf("ok!\n");
        }
#else
        kill(ppid[k], SIGUSR1);
#endif
    }
    close(aiocbp->aio_fildes);
    return 0;
}

int main(int argc, char* argv[])
{
    struct aiocb   cb;
    int            ppidlen = 0;
    pid_t        * ppid    = malloc(sizeof(pid_t) * (argc - 1));
    ssize_t        pbufrd  = 0;
    int            i, write_msqid;
    size_t         anz, i2;
    ssize_t        brcvd, j, lim;
    char         * buffer;
    struct msgbuf  mymsgbuf;

    if (signal(SIGTERM, aiosrv_sighandler) == SIG_ERR)
    {
        printf("there be dragons eating signals and numbers");
        exit(1);
    }

    if (signal(SIGUSR1, aiosrv_sighandler2) == SIG_ERR)
    {
        printf("there be dragons eating signals and numbers");
        exit(1);
    }

    if (argc < 2)
    {
        printf("This program is not intended for usage by the real user");
        exit(1);
    }

    if (ppid == NULL)
    {
#ifdef DEBUG
        printf("Insufficient memory available!\n");
        if (kill(ppid[0], SIGUSR2) == -1)
        {
            printf("My father died! Buhuhuhuhu!!!\n");
        }
#else
        kill(ppid[0], SIGUSR2);
#endif
        exit(1);
    }

    for (i = 1; i < argc; i ++)
    {
        if ((ppid[i - 1] = atoi(argv[i])) < 1)
        {
            printf("Invalid argument, expected positive integer value");
            exit(1);
        }
        else
        {
            ppidlen ++;
        }
    }

    if (kill(ppid[0], SIGUSR1) == -1)
    {
        perror("father died or insufficient permissions, i will also die now!");
        exit(1);
    }

    pbufrd = read(0, &cb, sizeof(struct aiocb));
    if (pbufrd == (ssize_t) -1)
    {
        perror("Error while reading the cb: definitely missing bytes");
#ifdef DEBUG
        if (kill(ppid[0], SIGUSR2) == -1)
        {
            printf("My father died! Buhuhuhuhu!!!\n");
        }
#else
        kill(ppid[0], SIGUSR2);
#endif
        return 1;
    }

    if (pbufrd != sizeof(struct aiocb))
    {
        perror("Error while reading the cb: missing bytes");
#ifdef DEBUG
        if (kill(ppid[0], SIGUSR2) == -1)
        {
            printf("My father died! Buhuhuhuhu!!!\n");
        }
#else
        kill(ppid[0], SIGUSR2);
#endif
        return 1;
    }

    //printf("aiosrv: cb.aio_lio_opcode: %d\n", cb.aio_lio_opcode);
    switch (cb.aio_lio_opcode)
    {
        case O_WRITE:
            // get the buffer stuff
            buffer = (char *) malloc(cb.aio_nbytes * sizeof(char));
            if ((write_msqid  = msgget(WSCHLUESSEL, IPC_CREAT | 0600)) == -1)
            {
#ifdef DEBUG
                if (kill(ppid[0], SIGUSR2) == -1)
                {
                    printf("My father died! Buhuhuhuhu!!!\n");
                }
#else
                kill(ppid[0], SIGUSR2);
#endif
            }

            anz = (int)(cb.aio_nbytes / PLEN + 1);

            errno = 0;
            for (i2 = 0; i2 < anz; i2++)
            {
                if ((brcvd = msgrcv(write_msqid, &mymsgbuf, sizeof(struct msgbuf), 1, 0)) == -1)
                {
                    perror("aiosrv: fail");
                }
                lim = cb.aio_nbytes < ((size_t)brcvd - sizeof(mymsgbuf.mtype))
                    ? (ssize_t)cb.aio_nbytes
                    : (brcvd - (ssize_t)sizeof(mymsgbuf.mtype));
                for (j = 0; j < lim; j++)
                {
                    buffer[i2 * PLEN + j] = mymsgbuf.mtext[j];
                }
            }

            cb.aio_buf = buffer;

            // tell the father, we got the data!
            if (kill(ppid[0], SIGUSR1) == -1)
            {
                // i guess, we can ignore an error here
            }
            pause();

            aiosrv_write(ppid, ppidlen, &cb);
            break;
        case O_READ:
            // tell the father, we got the data!
            if (kill(ppid[0], SIGUSR1) == -1)
            {
                // i guess, we can ignore an error here
            }
            pause();

            aiosrv_read(ppid, ppidlen, &cb);
            break;
        default:
            // send a message maybe?
#ifdef DEBUG
            if (kill(ppid[0], SIGUSR2) == -1)
            {
                printf("My father died! Buhuhuhuhu!!!\n");
            }
#else
            kill(ppid[0], SIGUSR2);
#endif
            return 1;
    }

    return 0;
}

