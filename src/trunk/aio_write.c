#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <sys/unistd.h>
#include <sys/msg.h>

#include "aio.h"
#include "aiosrv.h"
#include "aioinit.h"
#include "aio_util.h"
#include "aio_rw_shared.h"

// Define the key for the msg queue for transmitting aio_buf between the
// aio_write() call and the aiosrv process.
#define WSCHLUESSEL (key_t) 38266095

/**
 * Tries to execute a given write-operation according to the contents
 * in the control block addressed by aiocbp.
 * 
 * Returns 0 on success and -1 on failure. If a failure occurrs, errno will be
 * set appropriately:
 *  EINVAL - If the given control block is a NULL pointer, or if
 *           aiocbp->aio_offset is less than 0, or if aiocbp->aio_nbytes is
 *           less than 1.
 *  EBADF  - If aiocbp->aio_fildes is not a valid file descriptor or if the
 *           file denoted by aiocbp->aio_fildes is not opened for writing.
 *  EAGAIN - If the operation could currently not be processed because of
 *           lacking system ressources.
 */
int aio_write(struct aiocb *aiocbp)
{
    struct aiocb * headold;             // The old head of the global queue
    sig_t          sigold2;             // The old SIGUSR2 handler
    pid_t          sohn;                // The process id of the forked child
    int            pfd[2];              // Pipe input and output fd's
    int            pf2[2];              // Second pipe for exchanging codes
    char           pf2buff[24];         // A buffer for the pipe fd
    char           pidbuff1[24];        // Buffer for a process id in ASCII
    char           pidbuff2[24];        // Buffer for a process id in ASCII
    int            success = 0;         // Return value of aio_write()
    int            cdupfd;              // Duplicated file descriptor (in child)
    pid_t          ppid;                // The parent process id (used in child)
    int            write_msqid;         // The id of the msq used between
                                        // aio_write() and aiosrv
    struct msgbuf  wmsgbuf;             // Buffer for the msq above
    int            fflags;              // Flags of the aiocbp->aio_fildes
    size_t         chunk_len = PLEN;    // Length of chunk to send via the msq
    int            chunk_cnt = 0;       // Current chunk count


    if (aiocbp == NULL)
    {
        // NULL pointer is trash!
        errno = EINVAL;
        return -1;
    }

    // Test if aiocbp->aio_fildes is a valid file descriptor and get the flags
    // provided with the open() syscall.
    if ((fflags = fcntl(aiocbp->aio_fildes, F_GETFL)) == -1 && errno == EBADF)
    {
        aio_pdebug("%s (%d): Getting flags of file descriptor failed\n",
                   __FILE__, __LINE__);
        // errno = EBADF; // errno is already EBADF
        return -1;
    }

    // Test whether the file is opened in writable mode.
    if ((fflags & O_WRONLY) != O_WRONLY  && (fflags & O_RDWR) != O_RDWR)
    {
        aio_pdebug("%s (%d): File is not opened for writing\n", __FILE__,
                   __LINE__);
        errno = EBADF;
        return -1;
    }

    // Test whether aiocbp->aio_offset and aiocbp->aio_nbytes are valid (i.e.
    // positive numbers, at least zero for aio_offset).
    if (aiocbp->aio_offset < 0 || aiocbp->aio_nbytes <= 0)
    {
        aio_pdebug("%s (%d): aiocbp->aio_offset (value %d) or "
                   "aiocbp->aio_nbytes (value %d) is negative\n",
                   __FILE__, __LINE__,
                   aiocbp->aio_offset,
                   aiocbp->aio_nbytes);
        errno = EINVAL;
        return -1;
    }

    /*
    // This code does not compile for some reasons...
    if (fstat(aiocbp->aio_fildes, &fstat) == -1)
    {
        // fstat may set errno to EBADF, EOVERFLOW or EIO
        return -1;
    }

    if (aiocbp->aio_offset >= fstat.st_size)
    {
        errno = EOVERFLOW;
        return -1;
    }
    */

    // Try to open the pipe for communicating with the child.
    // The contents of the aiocbp control block will be transferred through
    // this pipe.
    if (pipe(pfd) == -1)
    {
        errno = EAGAIN;
        return -1;
    }
    
    // Create a pipe which can we reuse throughout the application lifetime.
    if (_aio_rw_sigmsgpfdin == 0 && _aio_rw_sigmsgpfdout == 0)
    {
        if (pipe(pf2) == -1)
        {
            errno = EAGAIN;
            return -1;
        }

        _aio_rw_sigmsgpfdin  = pf2[0];
        _aio_rw_sigmsgpfdout = pf2[1];
    }

    // Register aio_write_sighandler on USR1 and USR2 signals.
    if ((sigold2 = signal(SIGUSR2, aio_rw_sighandler)) == SIG_ERR)
    {
        // should not happen

        errno = EAGAIN;
        return -1;
    }

    // Woohoo, let's spoon! Aahh.. knife! I mean FORK!
    switch (sohn = fork())
    {
        case -1:
            // Fork failed due to too less system resources.
            errno = EAGAIN;
            return -1;
        case 0: // child process
            ppid = getppid();
            switch (fork())
            {
                case -1:
                    // This means still an error in processing. Signal the
                    // parent to cancel the operation and return -1 and an
                    // appropriate errno. We do not check for success of the
                    // kill() call, because an error could mean that the
                    // parent process already died.
                    aio_rw_send(_aio_rw_sigmsgpfdout, ppid, AIOSRV_ERROR);
                    while (close(pfd[0]) == -1 && errno == EINTR) {} // repeat
                    exit(1);
                    break;
                case 0: //child child process
                    // We don't need the pipe here, so close it.
                    while (close(pfd[0]) == -1 && errno == EINTR) {} // repeat
                    while (close(pfd[1]) == -1 && errno == EINTR) {} // repeat

                    // Try to get the message queue identifier.
                    if ((write_msqid = msgget(WSCHLUESSEL, IPC_CREAT | 0600))
                            == -1)
                    {
                        aio_perror("%s (%d): Failed getting the message queue",
                                   __FILE__, __LINE__);
                        aio_rw_send(_aio_rw_sigmsgpfdout, ppid, AIOSRV_ERROR);
                        exit(1);
                    }

                    // Split the message in aiocbp->aio_buf to chunks of a
                    // length of chunk_len bytes. Count the chunks.
                    while (chunk_cnt * chunk_len < aiocbp->aio_nbytes)
                    {
                        if (aiocbp->aio_nbytes < (chunk_cnt + 1) * chunk_len)
                        {
                            // We reached the last chunk! Adjust chunk_len to
                            // get a few bytes smaller, so we don't transfer
                            // random byte stuff.
                            chunk_len = aiocbp->aio_nbytes - chunk_cnt *
                                chunk_len;
                        }

                        // Prepare the msgbuf
                        wmsgbuf.mtype = getpid();
                        memcpy(wmsgbuf.mtext,
                               aiocbp->aio_buf,
                               chunk_len);
                        aio_pdebug("%s (%d): Sending a message...\n",
                                   __FILE__, __LINE__);
                        if (msgsnd(write_msqid, &wmsgbuf,
                                   sizeof(struct msgbuf), 0) == -1)
                        {
                            aio_perror("%s (%d): Failed sending a message",
                                       __FILE__, __LINE__);
                            aio_rw_send(_aio_rw_sigmsgpfdout, ppid, AIOSRV_ERROR);
                            exit(1);
                        }

                        chunk_cnt ++;
                    }
                    aio_pdebug("%s (%d): Ready.\n",
                               __FILE__, __LINE__);
                    exit(0);
                    // We are done! The contents of aiocbp->aio_buf should be
                    // there at the other side (aiosrv).
                    break;
                default: //child process
                    // We don't need the pipe output end here, so close it.
                    // Do the same for stdin, we wan't to override it with
                    // the pipe input.
                    while (close(pfd[1]) == -1 && errno == EINTR) {} // repeat
                    while (close(0)      == -1 && errno == EINTR) {} // repeat

                    // Duplicate the pipe input. Hopefully it will get fd 0,
                    // as we want to read the control block contents from
                    // stdin in aiosrv.
                    if ((cdupfd = dup(pfd[0])) == -1 && errno == EMFILE)
                    {
                        aio_pdebug("%s (%d): Maximum number of file "
                                   "descriptors reached",
                                  __FILE__, __LINE__);
                        aio_rw_send(_aio_rw_sigmsgpfdout, ppid, AIOSRV_ERROR);
                        // error would mean, that the parent is either already
                        // dead, than we can ignore it and die, or that the
                        // signal is wrong (it's definitely not) or that the 
                        // parent process is running under a different user
                        // account (should NEVER be the case, except for
                        // setuid)
                        while (close(pfd[0]) == -1 && errno == EINTR) {} 
                        exit(1);
                    }

                    // It was not dup'd to stdin. :(
                    if (cdupfd != 0)
                    {
                        // Signal parent of failure, close pipe input and the
                        // duplicated pipe input fd.
                        aio_perror("%s (%d): Tux or Beasty does not like to "
                                   "override the stdin.",
                                   __FILE__, __LINE__);
                        aio_rw_send(_aio_rw_sigmsgpfdout, ppid, AIOSRV_ERROR);
                        while (close(pfd[0]) == -1 && errno == EINTR) {}
                        while (close(cdupfd) == -1 && errno == EINTR) {}
                        exit(1);
                    }

                    //schreibe VatperprocessID in pidbuff
                    if (aiocbp->aio_lio_opcode == O_WRITE && aiocbp->aio_errno > 0)
                    {
                        // it was a LIO_NOWAIT list-call! we have to tell
                        // aiosrv, which pid numbers it has to signal after a
                        // operation is finished.
                        sprintf(pf2buff,  "%d", _aio_rw_sigmsgpfdout);
                        sprintf(pidbuff1, "%d", getppid());
                        sprintf(pidbuff2, "%d", aiocbp->aio_errno);
                        aio_pdebug("%s (%d): Executing `aiosrv %s %s %s`...\n",
                                   __FILE__, __LINE__,
                                   pf2buff, pidbuff1, pidbuff2);
                        execlp("aiosrv", "aiosrv", pf2buff, pidbuff1, pidbuff2, NULL);
                    }
                    else
                    {
                        // It was a simple and direct call to aio_write().
                        // Tell aiosrv the pid of the calling parent process.
                        sprintf(pf2buff,  "%d", _aio_rw_sigmsgpfdout);
                        sprintf(pidbuff1, "%d", getppid());
                        aio_pdebug("%s (%d): Executing `aiosrv %s %s`...\n",
                                   __FILE__, __LINE__,
                                   pf2buff, pidbuff1);
                        execlp("aiosrv", "aiosrv", pf2buff, pidbuff1, NULL);
                    }

                    // This should be unreachable, but who knows...
                    // May be entered, if aiosrv is not in the $PATH env
                    // variable
                    aio_rw_send(_aio_rw_sigmsgpfdout, ppid, AIOSRV_ERROR);
                    while (close(pfd[0]) == -1 && errno == EINTR) {} // repeat
                    exit(1);
                    break;
            }
            break; 
        default:
            while (close(pfd[0]) == -1 && errno == EINTR) {} // repeat it
           
            // We only need to wait for the child, if we have not yet
            // received a SIGUSR2.
            if (_aio_rw_sigmsg == NULL)
            {
                aio_pdebug("%s (%d): Waiting for child response...\n",
                           __FILE__, __LINE__); 
                // We wait until we get a USR2 signal
                while (_aio_rw_sigmsg == NULL)
                {
                    pause(); // wait until the child is ready for receiving data
                }
                errno = 0;
                aio_pdebug("%s (%d): Continuing\n",
                           __FILE__, __LINE__);
            }
            else
            {
                aio_pdebug("%s (%d): Child is ready\n",
                           __FILE__, __LINE__);
            }

            if (_aio_rw_sigmsg->sigcode == AIOSRV_ERROR)
            {
                _aio_rw_sigmsg = _aio_rw_sigmsg->signext;
                aio_pdebug("%s (%d): Received an error condition from "
                           "aiosrv\n",
                           __FILE__, __LINE__);
            }
            else
            {
                _aio_rw_sigmsg = _aio_rw_sigmsg->signext;

                // Prepare the control block
                aiocbp->aio_pid        = sohn;
                aiocbp->aio_errno      = EINPROGRESS;
                aiocbp->aio_lio_opcode = O_WRITE;
    
                aio_pdebug("%s (%d): aiocbp->aio_pid = %d\n",
                           __FILE__, __LINE__,
                           aiocbp->aio_pid);
                aio_pdebug("%s (%d): aiocbp->aio_lio_opcode = %d\n",
                           __FILE__, __LINE__,
                           aiocbp->aio_lio_opcode);
    
                // Send the control block through the pipe. aiosrv is now reading.
                if (write(pfd[1], aiocbp, sizeof(struct aiocb)) == -1)
                {
                    aio_perror("%s (%d): Sending the control block to the child "
                               "failed.",
                               __FILE__, __LINE__); 
                    errno = EAGAIN;
                    success = -1;
                }
    
                // If we have not yet received another USR1 or a USR2, we have to
                // wait for one of these. SIGUSR2 would indicate that an error
                // was raised before forking or while receiving the required data
                // in aiosrv.
                if (_aio_rw_sigmsg == NULL)
                {
                    aio_pdebug("%s (%d): Waiting for child response...\n",
                               __FILE__, __LINE__);
                    // We wait until we get a USR2 signal
                    while (_aio_rw_sigmsg == NULL)
                    {
                        pause(); // wait until the child is ready for receiving data
                    }
                    errno = 0;
                    aio_pdebug("%s (%d): Continuing\n",
                               __FILE__, __LINE__);
                }
                else
                {
                    aio_pdebug("%s (%d): Child is ready\n",
                               __FILE__, __LINE__);
                }

                if (_aio_rw_sigmsg->sigcode == AIOSRV_ERROR)
                {
                    aio_pdebug("%s (%d): Child died unexpectedly!\n",
                               __FILE__, __LINE__);
                    errno = EAGAIN; // maybe another try could help
                    success = -1;
                }

                _aio_rw_sigmsg = _aio_rw_sigmsg->signext;
            }
            
            // Reset the signal handlers.
            if (signal(SIGUSR2, sigold2) == SIG_ERR)
            {
                // should not happen
                aio_perror("%s (%d): Failed resetting the USR2 signal service"
                           " routine",
                           __FILE__, __LINE__);
                errno = EAGAIN;
                return -1;
            }


            // Child is still waiting here for being allowed to do the stuff.
            if (success == 0)
            {
                aio_pdebug("%s (%d): Let the child do the work...\n",
                           __FILE__, __LINE__);
                if (kill(sohn, SIGUSR1) == -1)
                {
                    aio_perror("%s (%d): Could not send USR1 to the child",
                               __FILE__, __LINE__);
                }
            }

            while (close(pfd[1]) == -1 && errno == EINTR) {} // repeat it

            // Also see the great debug messages for more information. :-)
            aio_pdebug("%s (%d): Enqueing the control block to the global "
                       "list\n",
                       __FILE__, __LINE__);
            headold = HeadPtr;
            HeadPtr = aiocbp;
            HeadPtr->aio_next = headold;

            break;
    }

    return success; // && hope_that(success == 0);
}

