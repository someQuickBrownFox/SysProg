#include "aio.h"
#include <errno.h>

static int aio_suspend_isready(struct aiocb *list[], int nent){
    int i;
    for (i=0; i<nent; i++ ){
	if ((list[i]->aio_errno)!=EINPROGRESS){
	    return 1;	  
	}
    }
    return 0;
}


int aio_suspend (struct aiocb *list[], int nent, struct timespec *timeout) {
	
	sigset_t set, oldset;
	sigfillset(&set);
	
	sigprocmask(SIG_SETMASK, &set, &oldset);
	
	if (aio_suspend_isready(list, nent)){
	    sigprocmask(SIG_SETMASK, &oldset, 0);
	    return 0;
	}

	int ret=sigtimedwait(&set, 0, timeout);
		
	sigprocmask(SIG_SETMASK, &oldset, 0);
	
	if (ret==-1 && errno==EAGAIN){ 
	    // Timeout occured.
	    return -1; // errno already EAGAIN.
	}
	
	if (ret>0) {
	    // We have caught a blocked signal. We have to forward it.
	    raise (ret);
	}
	
	if (aio_suspend_isready(list, nent)) {
	    // At least one operation is finished. 
	    return 0;
	}
	else {
	    // Interrupted by signal.
	    errno =EINTR;
	    return -1;
	}
		
}

