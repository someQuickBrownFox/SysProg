#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <sys/unistd.h>

#include "aio_util.h"
#include "aio_rw_shared.h"

struct aio_rw_sigmsg * _aio_rw_sigmsg       = NULL;
int                    _aio_rw_sigmsgpfdin  = 0;
int                    _aio_rw_sigmsgpfdout = 0;

/**
 * Defines a simple function which serves as the signal handler internally
 * used by the aio_read() and aio_write() functions. After a signal of type
 * SIGUSR2 is received the corresponding aio_rw_sigmsg struct will be appended
 * to the global queue with the head of _aio_rw_sigmsg.
 */
void aio_rw_sighandler(int signo)
{
    signal(signo, aio_rw_sighandler);
    struct aio_rw_sigmsg * received_signal = (struct aio_rw_sigmsg *)
        malloc(sizeof(struct aio_rw_sigmsg));
    if (received_signal == NULL)
    {
        aio_pdebug("%s (%d): Failed allocating memory for sigmsg. Exiting.\n",
                   __FILE__, __LINE__);
        exit(1);
    }

    struct aio_rw_sigmsg * qnext;
    
    received_signal->signo   = signo;
    received_signal->signext = NULL;

    if (read(_aio_rw_sigmsgpfdin, &(received_signal->sigcode),
             sizeof(received_signal->sigcode)) == -1)
    {
        aio_perror("%s (%d): Something went terribly wrong while trying to"
                   "read the signal code",
                   __FILE__, __LINE__);
    }

    if (_aio_rw_sigmsg == NULL)
    {
        _aio_rw_sigmsg = received_signal;
    }
    else
    {
        qnext = _aio_rw_sigmsg;
        while (qnext->signext != NULL)
        {
            qnext = qnext->signext;
        }
        qnext->signext = received_signal;
    }

    aio_pdebug("%s (%d): Received signal %d\n",
               __FILE__, __LINE__,
               signo);
}

/**
 * Sends a signal USR2 to the given process id and writes the code into the
 * file denoted by fd.
 */
int aio_rw_send(int fd, pid_t ppid, int code)
{
    aio_pdebug("%s (%d): Sending signal %d to process %d with code %d",
               __FILE__, __LINE__,
               SIGUSR2, ppid, code);
    if (write(fd, &code, sizeof(code)) == -1)
    {
        return -1;
    }

    if (kill(ppid, SIGUSR2) == -1)
    {
        return -1;
    }

    return 0;
}

