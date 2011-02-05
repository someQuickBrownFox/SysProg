#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <sys/unistd.h>
//#include <sys/stat.h>
#include <fcntl.h>

#include "aio.h"
#include "aiosrv.h"
#include "aioinit.h"
#include "aio_util.h"
#include "aio_rw_shared.h"

/**
 * Tries to execute a given read-operation according to the contents
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
int aio_read(struct aiocb * aiocbp)
{
    sig_t          sigold2;      // The original SIGUSR2 handler
    struct aiocb * headold;      // The head of the global queue
    int            pfd[2];       // Pipe fd: [0] pipe out; [1] pipe in
    int            pf2[2];       // Second pipe for exchanging codes
    pid_t          cpid;         // Process id of the forked child
                                 // from aiosrv to aio_read()
    char           pf2buff[24];  // A buffer for the pipe fd
    char           pidbuff1[24]; // A buffer for a process ID in ASCII
    char           pidbuff2[24]; // A buffer for a process ID in ASCII
    int            success = 0;  // The return value of this function
    int            cdupfd;       // File descriptor of the duplicated
                                 // pipe input, used for overwriting stdin of
                                 // aiosrv to pdf[0].
    int            fflags;       // The flags of the aiocbp->aio_fildes which
                                 // were given with the open() syscall
    /*struct stat    fstat;*/

    aio_pdebug("%s (%d): Starting a read-operation of %x\n",
               __FILE__, __LINE__, (int) aiocbp);
    
    if (aiocbp == NULL)
    {
        // NULL == TRASH
        errno = EINVAL;
        return -1;
    }

    // Check if aio_fildes is a valid file descriptor and get the flags.
    if ((fflags = fcntl(aiocbp->aio_fildes, F_GETFL)) == -1 && errno == EBADF)
    {
        aio_pdebug("%s (%d): Getting flags of file descriptor failed\n",
                   __FILE__, __LINE__);
        // errno = EBADF; // errno is already EBADF
        return -1;
    }

    // Test whether the file is opened for reading.
    if ((fflags & O_RDONLY) != O_RDONLY && (fflags & O_RDWR) != O_RDWR)
    {
        aio_pdebug("%s (%d): File is not opened for reading\n", __FILE__,
                   __LINE__);
        errno = EBADF;
        return -1;
    }

    // Test if aio_offset and aio_nbytes contain valid values.
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
    // does not work, fstat not defined... (?!)
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

    // create unnamed pipe
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

    // We need this signal for synchronizing the data transfer from parent to
    // child. Using the USR1 signal here for synchronizing could be a problem,
    // if the aiosrv-operation is completed before this routine finishes.
    if ((sigold2 = signal(SIGUSR2, aio_rw_sighandler)) == SIG_ERR)
    {
        // should not happen
        errno = EAGAIN;
        return -1;
    }

    // Spoon! Knife! Fork!
    switch (cpid = fork())
    {
        case -1:
            // fork() failed due to system resource limits.
            errno = EAGAIN;
            return -1;
        case 0: // child process
            // pipe auf stdin umleiten und den nicht gebrauchten pipe-eingang
            // schließen
            while (close(pfd[1]) == -1 && errno == EINTR) {} // repeat it
            while (close(0)      == -1 && errno == EINTR) {} // repeat it

            // Try to duplicate the pipe input to stdin before finally
            // executing aiosrv.
            if ((cdupfd = dup(pfd[0])) == -1 && errno == EMFILE)
            {
                aio_pdebug("%s (%d): Maximum number of file descriptors "
                           "reached",
                           __FILE__, __LINE__);
                // maximum number of file descriptors reached
                aio_rw_send(_aio_rw_sigmsgpfdout, getppid(), AIOSRV_ERROR);
                // error would mean, that the parent is either already
                // dead, than we can ignore it and die, or that the
                // signal is wrong (it's definitely not) or that the parent
                // process is running under a different user account (should
                // NEVER be the case, except for setuid)
                while (close(pfd[0]) == -1 && errno == EINTR) {} // repeat it
                exit(1);
            }

            // Test if the duplicated fd is really on stdin
            if (cdupfd != 0)
            {
                aio_perror("%s (%d): tux/beasty doesn't like to override the "
                           "stdin",
                           __FILE__, __LINE__);
                aio_rw_send(_aio_rw_sigmsgpfdout, getppid(), AIOSRV_ERROR);
                while (close(pfd[0]) == -1 && errno == EINTR) {} // repeat it
                while (close(cdupfd) == -1 && errno == EINTR) {} // repeat it
                exit(1);
            }

            // Only lio_listio() should set aio_lio_opcode to an appropriate
            // value. If so, aio_errno is used for storing the process id of
            // the child in lio_listio(), if lio-mode is set to LIO_NOWAIT.
            // This way, aiosrv can signal the lio_listio() child, after a
            // operation has finished.
            if (aiocbp->aio_lio_opcode == O_READ && aiocbp->aio_errno > 0)
            {
                // it was a LIO_NOWAIT list-call! we have to tell aiosrv,
                // which pid numbers it has to signal after a operation is
                // finished. aiosrv does not care about aiocbp->aio_errno
                // to be a real process id. So if a user sets aio_lio_opcode
                // to a meaningful and correct value, and aio_errno to some
                // process id, he could abuse this to kill some of his
                // processes. But honestly, who would do so???
                sprintf(pf2buff,  "%d", _aio_rw_sigmsgpfdout);
                sprintf(pidbuff1, "%d", getppid());
                sprintf(pidbuff2, "%d", aiocbp->aio_errno);
                aio_pdebug("%s (%d): Executing `aiosrv %s %s %s`\n",
                           __FILE__, __LINE__,
                           pf2buff, pidbuff1, pidbuff2);
                execlp("aiosrv", "aiosrv", pf2buff, pidbuff1, pidbuff2, NULL);
            }
            else
            {
                // Simple, direct call.
                sprintf(pf2buff,  "%d", _aio_rw_sigmsgpfdout);
                sprintf(pidbuff1, "%d", getppid());
                aio_pdebug("%s (%d): Executing `aiosrv %s %s`\n",
                           __FILE__, __LINE__,
                           pf2buff, pidbuff1);
                execlp("aiosrv", "aiosrv", pf2buff, pidbuff1, NULL);
            }

            // should not be reached, but could mean an error case
            aio_pdebug("%s (%d): Failed executing aiosrv\n",
                       __FILE__, __LINE__);
            aio_rw_send(_aio_rw_sigmsgpfdout, getppid(), AIOSRV_ERROR);
            while (close(pfd[0]) == -1 && errno == EINTR) {} // repeat it
            exit(1);
            break;
        default:
            // close pipe input
            while (close(pfd[0]) == -1 && errno == EINTR) {} // repeat it

            // If _sigusr1cnt is greater than 0, we already received a signal
            // from aiosrv, so we dont have to wait again for one. The first
            // signal would mean, the child is ready to receive the control
            // block. The second signal tells us, if the child received the
            // data correctly (USR1) or if it misses stuff or occurred some
            // errors (USR2). After the second USR1 from aiosrv, we have to
            // send a SIGUSR1 to aiosrv, to tell it to finally start the
            // operation. If we would not use this mechanism, strange things
            // would happen. We already learned this during testing.
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
                aio_pdebug("%s (%d): Continuing\n", __FILE__, __LINE__);
            }
            else
            {
                aio_pdebug("%s (%d): Child is ready\n",
                           __FILE__, __LINE__);
            }

            if (_aio_rw_sigmsg->sigcode == AIOSRV_ERROR)
            {
                _aio_rw_sigmsg = _aio_rw_sigmsg->signext;

                aio_pdebug("%s (%d): aiosrv responded with an error\n",
                           __FILE__, __LINE__);
                errno = EAGAIN;
                success = -1;
            }
            else
            {
                _aio_rw_sigmsg = _aio_rw_sigmsg->signext;

                // Prepare the control block
                aiocbp->aio_pid        = cpid;
                aiocbp->aio_lio_opcode = O_READ;
                aiocbp->aio_errno      = EINPROGRESS;
    
                aio_pdebug("%s (%d): aiocbp->aio_pid == %d\n",
                           __FILE__, __LINE__, aiocbp->aio_pid);
                aio_pdebug("%s (%d): aiocbp->aio_lio_opcode = %d\n",
                           __FILE__, __LINE__, aiocbp->aio_lio_opcode);

                // Send the control block through the pipe.
                if (write(pfd[1], aiocbp, sizeof(struct aiocb)) == -1)
                {
                    aio_pdebug("%s (%d): Sending the control block to the child "
                               "failed.\n",
                               __FILE__, __LINE__);
                    errno = EAGAIN;
                    success = -1;
                }
    
                // If we don't have received a second USR1 or a first USR2 yet,
                // we have to wait for one.
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
                    aio_pdebug("%s (%d): Continuing\n", __FILE__, __LINE__);
                }
                else
                {
                    aio_pdebug("%s (%d): Child is ready\n",
                               __FILE__, __LINE__);
                }

                // USR2 means error, so tell it.
                if (_aio_rw_sigmsg->sigcode == AIOSRV_ERROR)
                {
                    aio_pdebug("%s (%d): Child died unexpectedly\n",
                               __FILE__, __LINE__);
                    errno = EAGAIN;
                    success = -1;
                }

                _aio_rw_sigmsg = _aio_rw_sigmsg->signext;
            }
    
            // Try to reset the signal handlers for USR2.
            if (signal(SIGUSR2, sigold2) == SIG_ERR)
            {
                // should not happen
                aio_perror("%s (%d): Failed resetting the USR2 signal service"
                           " routine", __FILE__, __LINE__);
                errno = EAGAIN;
                return -1;
            }

            // do this only, if things till there gone right.
            if (success == 0)
            {
                aio_pdebug("%s (%d): Let the child do the work...\n",
                           __FILE__, __LINE__);
                if (kill(cpid, SIGUSR1) == -1)
                {
                    aio_perror("%s (%d): Could not send USR1 to the child\n",
                               __FILE__, __LINE__);
                }
            }


            while (close(pfd[1]) == -1 && errno == EINTR) {} // repeat it

            // enqueue the control block if stuff was going right
            aio_pdebug("%s (%d): Enqueing the aiocbp to the global list\n",
                       __FILE__, __LINE__);
            headold = HeadPtr;
            HeadPtr = aiocbp;
            HeadPtr->aio_next = headold;

            break;
    }

    return success;
}

