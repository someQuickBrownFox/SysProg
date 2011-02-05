struct aio_rw_sigmsg {
    int                    signo;
    int                    sigcode;
    struct aio_rw_sigmsg * signext;
};

extern struct aio_rw_sigmsg * _aio_rw_sigmsg;
extern int                    _aio_rw_sigmsgpfdout;
extern int                    _aio_rw_sigmsgpfdin;

/**
 * Defines a simple function which serves as the signal handler internally
 * used by the aio_read() and aio_write() functions. After a signal of type
 * SIGUSR2 is received the corresponding aio_rw_sigmsg struct will be appended
 * to the global queue with the head of _aio_rw_sigmsg.
 */
void aio_rw_sighandler(int signo);

/**
 * Sends a signal USR2 to the given process id and writes the code into the
 * file denoted by fd.
 */
int aio_rw_send(int fd, pid_t ppid, int code);

