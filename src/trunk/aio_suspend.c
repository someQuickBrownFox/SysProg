// ////////////////////////////////////////////////////////////////////////////////////
// Systemprogrammierung unter UNIX
// WS 2010/11
// Cristea Irina
// Rotsch Matthias
// ////////////////////////////////////////////////////////////////////////////////////

#include "aio.h"
#include <errno.h>

// ////////////////////////////////////////////////////////////////////////////////////
// function aio_suspend_isready
// Checks whether one list entry represents an aio operation
// that is finished (not "in progress").
// params:
//  list      array of pointers to aiocb struct's
//  nent      length of array
// returns:
//  0         nothing finished
//  1         at least one operation finished
// NB: - List entries that are NULL are ignored.
//     - If list itself is NULL and nent is > 0, then we crash. This
//       case is not covered in specs of aio_suspend either.
// ////////////////////////////////////////////////////////////////////////////////////

static int aio_suspend_isready(struct aiocb *list[], int nent){
    int i;
    for (i=0; i<nent; i++ ) {
	if ( list[i]!=0 && (list[i]->aio_errno)!=EINPROGRESS) {
	    // found a finished op
	    return 1;
	}
    }
    // no op finished
    return 0;
}

// ////////////////////////////////////////////////////////////////////////////////////
// function aio_suspend
// Suspends caller until
//  at least one aio operation given is finished,
//  or a signal occurs,
//  or (optionally) until timeout is exceeded.
// See aio_suspend(3) for details.
// params:
//  list      array of pointers to aiocb struct's
//  nent      length of array
//  timeout   wait at most this long. May be NULL, then we may wait infinitely.
// returns:
//  0         at least one operation finished
//  -1        otherwise, errno is set to
//            EAGAIN: timeout exceeded, but no operation finished
//            EINTR:  blocking call was interrupted by signal, and 
//                    no operation is finished (NB: a signal could have notified
//                    a regular end of one operation. In this case we return 0)
//            EINVAL: Invalid timeout specified. ( Extension to aio_suspend(3) )
// ////////////////////////////////////////////////////////////////////////////////////

int aio_suspend (struct aiocb *list[], int nent, struct timespec *timeout) {

	int ret;
	sigset_t set, oldset;
	sigfillset(&set);

	// we block all signals
	sigprocmask(SIG_SETMASK, &set, &oldset);

	// check whether one op is finished
	if (aio_suspend_isready(list, nent)){
	    // yes, we restore block mask and return
	    sigprocmask(SIG_SETMASK, &oldset, 0);
	    return 0;
	}

	// wait for signal to occur, or for timeout
	// if timeout is NULL, then sigtimedwait doesn't end on timeout,
	// at least on linux.
	ret = sigtimedwait(&set, 0, timeout);

	// we will return, so first we will have to restore block mask
	sigprocmask(SIG_SETMASK, &oldset, 0);
	
	if ( ret==-1 && (errno==EAGAIN || errno==EINVAL) ){ 
	    // Timeout occured, or timeout parameter was invalid.
	    // We forward this to caller.
	    // NB: specs of aio_suspend do not cover the case that
	    // timeout might be invalid. Nevertheless, it is better
	    // to fail with proper error than to do unexpected things.
	    return -1; // errno already EAGAIN or EINVAL
	}
	
	if (ret>0) {
	    // Caught a signal blocked by block mask. We have to raise it again
	    // or the caller's intended signal action is not performed.
	    raise (ret);
	}

	// Now we check for finished operation again. The caught signal might
	// have ended an operation.
	if (aio_suspend_isready(list, nent)) {
	    // At least one operation is finished. 
	    return 0;
	}
	else {
	    // The signal that interrupted sigtimedwait had not ended
	    // an operation. We notify caller.
	    errno =EINTR;
	    return -1;
	}
		
}

// eof ////////////////////////////////////////////////////////////////////////////////
