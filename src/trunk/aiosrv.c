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
#include "aio_util.h"

#define WSCHLUESSEL (key_t) 38266095

int _sigusr1cnt = 0;

void aiosrv_sighandler(int signo)
{
    // Let the compiler believe, we are using signo
    signal(signo, aiosrv_sighandler);
    aio_pdebug("%s (%d): Terminating...\n", __FILE__, __LINE__, signo);
    close(0);
    exit(0);
}

void aiosrv_sighandler_resumer(int signo)
{
    // Let the compiler believe, we are using signo
    signal(signo, aiosrv_sighandler_resumer);
    if (signo == SIGUSR1)
    {
        _sigusr1cnt ++;
    }
    aio_pdebug("%s (%d): Signal %d received.\n", __FILE__, __LINE__, signo);
}

int aiosrv_read(pid_t ppid[], int ppidlen, struct aiocb * aiocbp)
{
    struct msgbuf  msgbuf;
    int            msqid;
    int            ppididx;

    int            elen          = sizeof(errno);
    int            slen          = sizeof(ssize_t);

    ssize_t        chunk_len_cur = 0;
    size_t         chunk_len     = PLEN - elen - slen;
    char         * chunk         = (char *) malloc((chunk_len + 1)
                                     * sizeof(char));
    int            chunk_cnt     = 0;
        
    if ((msqid = msgget(SCHLUESSEL, IPC_CREAT | 0600)) == -1)
    {
        aio_perror("%s (%d): Failed getting the message queue identifier",
                   __FILE__, __LINE__);
        close(aiocbp->aio_fildes);
        free(chunk);
        return -1;
    }

    // Set the read cursor to the desired position.
    if (lseek(aiocbp->aio_fildes, aiocbp->aio_offset, SEEK_SET) == (off_t)-1)
    {
        aio_perror("%s (%d): Failed setting the read cursor",
                   __FILE__, __LINE__);
        msgbuf.mtype = getpid();
        memcpy(msgbuf.mtext,
               &errno,
               elen);
        if (msgsnd(msqid, &msgbuf, sizeof(struct msgbuf), 0) == -1)
        {
            aio_perror("%s (%d): Failed sending the message",
                       __FILE__, __LINE__);
            close(aiocbp->aio_fildes);
            free(chunk);
            return -1;
        }

        for (ppididx = 0; ppididx < ppidlen; ppididx ++)
        {
            if (kill(ppid[ppididx], SIGUSR1) == -1)
            {
                aio_perror("%s (%d): Failed sending a signal to process %d",
                           __FILE__, __LINE__,
                           ppid[ppididx]);
            }
        }

        close(aiocbp->aio_fildes);
        free(chunk);
        return -1;
    }

    while (chunk_cnt * chunk_len < aiocbp->aio_nbytes)
    {
        if (aiocbp->aio_nbytes < (chunk_cnt + 1) * chunk_len)
        {
            chunk_len = aiocbp->aio_nbytes - chunk_cnt * chunk_len;
        }

        msgbuf.mtype = getpid();
        if ((chunk_len_cur = read(aiocbp->aio_fildes, chunk, chunk_len)) < 0)
        {
            // error
            aio_pdebug("%s (%d): Failed reading %d bytes\n",
                       __FILE__, __LINE__,
                       chunk_len);
            // put the errno
            memcpy(msgbuf.mtext,
                   &errno,
                   elen);
            // put the chunk size (negative value)
            memcpy(msgbuf.mtext,
                   &chunk_len_cur,
                   slen);

            if (msgsnd(msqid, &msgbuf, sizeof(struct msgbuf), 0) == -1)
            {
                aio_perror("%s (%d): Failed sending a message",
                           __FILE__, __LINE__);
                close(aiocbp->aio_fildes);
                free(chunk);
                return -1;
            }

            for (ppididx = 0; ppididx < ppidlen; ppididx ++)
            {
                aio_pdebug("%s (%d): Signaling process %d, we have "
                           "finished.\n",
                           __FILE__, __LINE__,
                           ppid[ppididx]);
                if (kill(ppid[ppididx], SIGUSR1) == -1)
                {
                    aio_perror("%s (%d): Failed sending signal",
                               __FILE__, __LINE__);
                }
            }
            
            close(aiocbp->aio_fildes);
            free(chunk);
            return -1;
        }
        else
        {
            // errno is 0 if this is the last chunk, EINPROGRESS otherwise.
            errno = ((chunk_cnt + 1) * chunk_len < aiocbp->aio_nbytes)
                  ? EINPROGRESS
                  : 0;

            // put the errno
            memcpy(msgbuf.mtext,
                   &errno,
                   elen);
            // put the chunk size
            memcpy(msgbuf.mtext + elen,
                   &chunk_len_cur,
                   slen);
            // put the chunk itself
            memcpy(msgbuf.mtext + elen + slen,
                   chunk,
                   chunk_len_cur);

            chunk[chunk_len_cur] = 0;
            aio_pdebug("%s (%d): Sending %d bytes via message queue: '%s'\n",
                       __FILE__, __LINE__,
                       chunk_len_cur, chunk);

            if (msgsnd(msqid, &msgbuf, sizeof(struct msgbuf), 0) == -1)
            {
                aio_perror("%s (%d): Sending message failed. Aborting...",
                           __FILE__, __LINE__);
                close(aiocbp->aio_fildes);
                free(chunk);
                return -1;
            }
        
            aio_pdebug("%s (%d): Signaling process %d, we have "
                       "finished sending a message\n",
                       __FILE__, __LINE__,
                       ppid[0]);    
            if (kill(ppid[0], SIGUSR1) == -1)
            {
                aio_perror("%s (%d): Sending signal failed. Aborting...",
                           __FILE__, __LINE__);
                close(aiocbp->aio_fildes);
                free(chunk);
                return -1;
            }
        }

        chunk_cnt ++;
    }

    for (ppididx = 1; ppididx < ppidlen; ppididx ++)
    {
        aio_pdebug("%s (%d): Signaling process %d, we have finished\n",
                   __FILE__, __LINE__);
        if (kill(ppid[ppididx], SIGUSR1) == -1)
        {
            aio_perror("%s (%d): Sending signal failed",
                       __FILE__, __LINE__);
        }
    }

    close(aiocbp->aio_fildes);
    free(chunk);
    return 0;
}

int aiosrv_write(pid_t ppid[], int ppidlen, struct aiocb * aiocbp)
{
    struct msgbuf  msgbuf;
    int            msqid;
    int            ppididx;
    ssize_t        bwritten;
    int            elen = sizeof(errno);
    int            slen = sizeof(ssize_t);

    if ((msqid = msgget(SCHLUESSEL, IPC_CREAT | 0600)) == -1)
    {
        aio_perror("%s (%d): Failed getting the message queue identifier. "
                   "Aborting...",
                   __FILE__, __LINE__);
        close(aiocbp->aio_fildes);
        return -1;
    }

    // Set the write cursor to the desired position.
    if (lseek(aiocbp->aio_fildes, aiocbp->aio_offset, SEEK_SET) == (off_t)-1)
    {
        aio_perror("%s (%d): Failed setting the write cursor",
                   __FILE__, __LINE__);
        msgbuf.mtype = getpid();
        memcpy(msgbuf.mtext,
               &errno,
               elen);
        if (msgsnd(msqid, &msgbuf, sizeof(struct msgbuf), 0) == -1)
        {
            aio_perror("%s (%d): Failed sending the message. Aborting...",
                       __FILE__, __LINE__);
            close(aiocbp->aio_fildes);
            return -1;
        }

        for (ppididx = 0; ppididx < ppidlen; ppididx ++)
        {
            if (kill(ppid[ppididx], SIGUSR1) == -1)
            {
                aio_perror("%s (%d): Failed sending a signal to process %d",
                           __FILE__, __LINE__,
                           ppid[ppididx]);
            }
        }

        close(aiocbp->aio_fildes);
        return -1;
    }

    errno    = 0;
    bwritten = write(aiocbp->aio_fildes, aiocbp->aio_buf, aiocbp->aio_nbytes);
    
    msgbuf.mtype = getpid();
    memcpy(msgbuf.mtext,
           &errno,
           elen);
    memcpy(msgbuf.mtext + elen,
           &bwritten,
           slen);
    if (msgsnd(msqid, &msgbuf, sizeof(struct msgbuf), 0) == -1)
    {
        aio_perror("%s (%d): Failed sending a message",
                   __FILE__, __LINE__);
        close(aiocbp->aio_fildes);
        return -1;
    }

    for (ppididx = 0; ppididx < ppidlen; ppididx ++)
    {
        if (kill(ppid[ppididx], SIGUSR1) == -1)
        {
            aio_pdebug("%s (%d): Sending USR1 to process %d failed\n",
                       __FILE__, __LINE__,
                       ppid[ppididx]);
        }
        else
        {
            aio_pdebug("%s (%d): Sending USR1 to process %d succeeded\n",
                       __FILE__, __LINE__,
                       ppid[ppididx]);
        }
    }

    close(aiocbp->aio_fildes);
    return 0;
}

int main(int argc, char* argv[])
{
    struct msgbuf  msgbuf;
    struct aiocb   cb;
    int            ppidlen = 0;
    pid_t        * ppid    = malloc(sizeof(pid_t) * (argc - 1));
    ssize_t        pbufrd  = 0;
    int            write_msqid;
    int            i;
    size_t         brcvd  = 0;
    int            mbrcvd = 0;

    if (signal(SIGTERM, aiosrv_sighandler) == SIG_ERR)
    {
        printf("there be dragons eating signals and numbers");
        free(ppid);
        exit(1);
    }

    if (signal(SIGUSR1, aiosrv_sighandler_resumer) == SIG_ERR)
    {
        printf("there be dragons eating signals and numbers");
        free(ppid);
        exit(1);
    }

    if (argc < 2)
    {
        printf("This program is not intended for usage by the real user");
        free(ppid);
        exit(1);
    }

    if (ppid == NULL)
    {
        aio_pdebug("%s (%d): Insufficient memory available\n",
                   __FILE__, __LINE__);
        if (kill(ppid[0], SIGUSR2) == -1)
        {
            aio_pdebug("%s (%d): Father already died, or insufficient "
                       "permissions. Exiting...\n",
                       __FILE__, __LINE__);
        }
        free(ppid);
        exit(1);
    }

    for (i = 1; i < argc; i ++)
    {
        if ((ppid[i - 1] = atoi(argv[i])) < 1)
        {
            printf("Invalid argument, expected positive integer value");
            free(ppid);
            exit(1);
        }
        else
        {
            ppidlen ++;
        }
    }

    aio_pdebug("%s (%d): Telling father, that we are ready for receiving "
               "the control block\n",
               __FILE__, __LINE__);
    if (kill(ppid[0], SIGUSR1) == -1)
    {
        aio_perror("%s (%d): Father already died, or insufficient "
                   "permissions. Exiting...",
                   __FILE__, __LINE__);
        free(ppid);
        exit(1);
    }

    pbufrd = read(0, &cb, sizeof(struct aiocb));
    if (pbufrd == (ssize_t) -1)
    {
        aio_perror("%s (%d): Error while reading the cb",
                   __FILE__, __LINE__);
        if (kill(ppid[0], SIGUSR2) == -1)
        {
            aio_perror("%s (%d): Father already died, or insufficient "
                       "permissions. Exiting...",
                       __FILE__, __LINE__);
        }
        free(ppid);
        exit(1);
    }

    if (pbufrd != sizeof(struct aiocb))
    {
        aio_pdebug("%s (%d): Error while reading the cb: missing bytes.\n",
                   __FILE__, __LINE__);
        if (kill(ppid[0], SIGUSR2) == -1)
        {
            aio_perror("%s (%d): Father already died, or insufficient "
                       "permissions. Exiting...",
                       __FILE__, __LINE__);
        }
        free(ppid);
        exit(1);
    }

    switch (cb.aio_lio_opcode)
    {
        case O_WRITE:
            aio_pdebug("%s (%d): Processing write operation...\n",
                       __FILE__, __LINE__);
            // get the buffer stuff
            cb.aio_buf = (char *) malloc(cb.aio_nbytes * sizeof(char));
            if ((write_msqid = msgget(WSCHLUESSEL, IPC_CREAT | 0600)) == -1)
            {
                aio_perror("%s (%d): Failed getting the message queue "
                           "identifier",
                           __FILE__, __LINE__);
                if (kill(ppid[0], SIGUSR2) == -1)
                {
                    aio_perror("%s (%d): Father already died, or insufficient"
                               " permissions. Exiting...",
                               __FILE__, __LINE__);
                }
            }

            aio_pdebug("%s (%d): Receiving the buffer contents...\n",
                       __FILE__, __LINE__);
            while (brcvd < cb.aio_nbytes)
            {
                if ((mbrcvd = msgrcv(write_msqid, &msgbuf,
                                     sizeof(struct msgbuf), 0, 0)) == -1)
                {
                    aio_perror("%s (%d): Failed receiving bytes from message",
                               "queue",
                               __FILE__, __LINE__);
                }
                aio_pdebug("%s (%d): Got a message!\n",
                           __FILE__, __LINE__);
                
                mbrcvd = mbrcvd - sizeof(msgbuf.mtype);
                if (brcvd + mbrcvd > cb.aio_nbytes)
                {
                    // received the last chunk
                    mbrcvd = cb.aio_nbytes - brcvd;
                }


                memcpy(cb.aio_buf + brcvd, msgbuf.mtext, mbrcvd);
                brcvd += mbrcvd;
            }

            // tell the father, we got the data!
            aio_pdebug("%s (%d): Signal father, we are ready for processing "
                       "the desired operation...\n",
                       __FILE__, __LINE__);
            if (kill(ppid[0], SIGUSR1) == -1)
            {
                // i guess, we can ignore an error here
                aio_pdebug("%s (%d): Parent already died!\n",
                           __FILE__, __LINE__);
            }
            else
            {
                aio_pdebug("%s (%d): Got the data from the parent! "
                           "Waiting to resume...\n",
                           __FILE__, __LINE__);
                if (_sigusr1cnt == 0)
                {
                    pause();
                }
                aio_pdebug("%s (%d): Resuming...\n", __FILE__, __LINE__);
            }

            aiosrv_write(ppid, ppidlen, &cb);
            free(cb.aio_buf);
            break;
        case O_READ:
            // tell the father, we got the data!
            if (kill(ppid[0], SIGUSR1) == -1)
            {
                // i guess, we can ignore an error here
                aio_pdebug("%s (%d): Parent already died!\n",
                           __FILE__, __LINE__);
            }
            else
            {
                aio_pdebug("%s (%d): Got the data from the parent! "
                           "Waiting to resume...\n",
                           __FILE__, __LINE__);
                if (_sigusr1cnt == 0)
                {
                    pause();
                }
                aio_pdebug("%s (%d): Resuming...\n", __FILE__, __LINE__);
            }

            aiosrv_read(ppid, ppidlen, &cb);
            break;
        default:
            // send a message maybe?
            if (kill(ppid[0], SIGUSR2) == -1)
            {
                // it's legal to send the parent the SIGUSR2 signal,
                // as it should still wait for USR1 or USR2 in the
                // aio_read() or aio_write() call.
                aio_pdebug("%s (%d): Father already died, or insufficient "
                           "permissions. Exiting...\n",
                           __FILE__, __LINE__);
            }
            free(ppid);
            return 1;
    }

    free(ppid);
    return 0;
}

