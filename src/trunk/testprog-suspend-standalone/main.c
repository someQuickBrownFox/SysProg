// ////////////////////////////////////////////////////////////////////////////////////
// Systemprogrammierung unter UNIX
// WS 2010/11
// Cristea Irina
// Rotsch Matthias
// ////////////////////////////////////////////////////////////////////////////////////

#include "../aio.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>


// globals - not nice, but fit for purpose
// /////////////////////////////////////////////////////////////

struct aiocb cb0;
struct aiocb cb2;
#define LISTSIZE 3
struct aiocb *list[LISTSIZE];

// this struct hold "expected" and "is" state of the current test
struct teststate {
    int	rc;		// return code of aio_suspend
    int	err;		// errno if rc was -1, 0 otherwise
    int	num_sigint;	// number of SIGINT seen during the test
    int num_sigalrm;	// number of SIGALRM seen during the test
};
struct teststate ts_exp;	// test should result in these values
struct teststate ts_is;		// actual values from test


// forward declarartions
// /////////////////////////////////////////////////////////////
void dotest(const char *txt, int timeout_ms, int signal_ms, int signo);
int check();
const char *errnostr(int);


// ////////////////////////////////////////////////////////////////////////////////////
// function my_signalhandler
// handles signals for this test:
// SIGALRM as the "something-is-ready" signal, in this case cb0.aio_errno is cleared
// SIGINT  as a "foreign" signal
// params:
//  n      number of signal
// ////////////////////////////////////////////////////////////////////////////////////
void my_sighandler(int n) {
    switch(n) {
      case SIGALRM:
	ts_is.num_sigalrm++;
	fprintf(stderr,"........ SIGALRM\n");
	// SIGALRM is used to perform an async end, we clear aio_errno
	cb2.aio_errno = 0;
	break;
      case SIGINT: 
	ts_is.num_sigint++; 
	fprintf(stderr,"........ SIGINT\n"); 
	break;
      default:
	fprintf(stderr,"OOPS: unexpected signal %d",n);
	exit(-1);
    }
}


// ////////////////////////////////////////////////////////////////////////////////////
// function main
// ////////////////////////////////////////////////////////////////////////////////////
int main() {
 
	printf("START testprog-suspend-standalone\n\n");

	// initialize globals
	memset(list,0,sizeof(list));
	list[0]=&cb0;
	list[2]=&cb2;

	// initialize sighandlers
	{
	    struct sigaction sa;
	    memset(&sa,0,sizeof(sa));
	    sa.sa_handler = my_sighandler;
	    sigaction(SIGALRM,&sa,0);
	    sigaction(SIGINT,&sa,0);
	}

	// perform tests

	// 1) invalid timeout
	cb0.aio_errno		= EINPROGRESS;
	cb2.aio_errno		= EINPROGRESS;
	ts_exp.rc		= -1;
	ts_exp.err		= EINVAL;
	ts_exp.num_sigint	= 0;
	ts_exp.num_sigalrm	= 0;
	dotest("invalid timeout",0,-1,0);
	
	// 2) op is done already
	cb0.aio_errno		= EINPROGRESS;
	cb2.aio_errno		= 0;
	ts_exp.rc		= 0;
	ts_exp.err		= 0;
	ts_exp.num_sigint	= 0;
	ts_exp.num_sigalrm	= 0;
	dotest("op is done initially",-1,-1,0);
	
	// 3) timeout in 500ms
	cb0.aio_errno		= EINPROGRESS;
	cb2.aio_errno		= EINPROGRESS;
	ts_exp.rc		= -1;
	ts_exp.err		= EAGAIN;
	ts_exp.num_sigint	= 0;
	ts_exp.num_sigalrm	= 0;
	dotest("timeout in 500ms",500,-1,0);
	
	// 4) no timeout, foreign signal in 500ms
	cb0.aio_errno		= EINPROGRESS;
	cb2.aio_errno		= EINPROGRESS;
	ts_exp.rc		= -1;
	ts_exp.err		= EINTR;
	ts_exp.num_sigint	= 1;
	ts_exp.num_sigalrm	= 0;
	dotest("no timeout, foreign signal in 500ms",-1,500,SIGINT);
	
	// 5) no timeout, finish signal in 500ms
	cb0.aio_errno		= EINPROGRESS;
	cb2.aio_errno		= EINPROGRESS;
	ts_exp.rc		= 0;
	ts_exp.err		= 0;
	ts_exp.num_sigint	= 0;
	ts_exp.num_sigalrm	= 1;
	dotest("no timeout, finish signal in 500ms",-1,500,SIGALRM);
	
	// 6) timeout in 500ms, finish signal in 1000ms
	cb0.aio_errno		= EINPROGRESS;
	cb2.aio_errno		= EINPROGRESS;
	ts_exp.rc		= -1;
	ts_exp.err		= EAGAIN;
	ts_exp.num_sigint	= 0;
	ts_exp.num_sigalrm	= 1;
	dotest("timeout in 500ms, finish signal in 1000ms",500,1000,SIGALRM);
	
	// 7) blocking forever
	cb0.aio_errno		= EINPROGRESS;
	cb2.aio_errno		= EINPROGRESS;
	ts_exp.rc		= -1;
	ts_exp.err		= EINTR;
	ts_exp.num_sigint	= 1;
	ts_exp.num_sigalrm	= 0;
	dotest("blocking forever (PRESS ctrl-C !!!!)",-1,-1,0);

/*
This test doesn't work, don't know why...
	// 8) aio_read from stdin
	memset(&cb0,0,sizeof(cb0));
	cb0.aio_fildes		= 0;
	char buf[1];
	cb0.aio_buf		= buf;
	cb0.aio_nbytes		= 1;
	int rc = aio_read(&cb0);
	if(rc) {
	    fprintf(stderr,"OOPS: aio_read has error %s\n",errnostr(errno));
	    exit(-1);
	}
	cb2.aio_errno		= EINPROGRESS;
	ts_exp.rc		= 0;
	ts_exp.err		= 0;
	ts_exp.num_sigint	= 0;
	ts_exp.num_sigalrm	= 0;
	dotest("aio_read from stdin (ENTER SOMETHING !!!!)",-1,-1,0);
*/
	
	printf("END testprog-suspend-standalone\n");

	return 0;
}


// ////////////////////////////////////////////////////////////////////////////////////
// function dotest
// Performs the actual test.
// params:
//  txt      	description to be displayed
//  timeout_ms	timeout in ms, if >0 the specified time is forwarded to aio_suspend
//                             if 0  an invalid value is given to aio_suspend
//                             if <0 no timeout
//  signal_ms   if >0 send the signal specified by signo in so many millisecs
//  signo       signal to be sent if signal_ms is >0
// returns:
//  <void>
// ////////////////////////////////////////////////////////////////////////////////////
void dotest(const char *txt, int timeout_ms, int signal_ms, int signo) {

    fprintf(stderr,"TEST BEGIN: \"%s\"\n",txt);

    // init state
    memset(&ts_is,0,sizeof(ts_is));

    int cld = -1;

    // in case we are expected to send a signal in the future,
    // we start a child for this task
    if(signal_ms>0) {
	cld = fork();
	if(!cld) {
	    // we are child: wait and send signal
	    usleep(1000*signal_ms);
	    kill(getppid(),signo);
	    exit(0);
	}
    }

    // are we using timeout?
    if(timeout_ms>=0) {
	struct timespec tspec;
	if(timeout_ms) {
	    // actual time given
	    tspec.tv_sec = timeout_ms / 1000;
	    tspec.tv_nsec = ( timeout_ms - tspec.tv_sec*1000 ) * 1000000;
	} else {
	    // NULL is a dummy value to indicate an invalid timespec
	    tspec.tv_sec = 999;
	    tspec.tv_nsec = 2000000000;
	}
	// do suspend
	ts_is.rc = aio_suspend(list,LISTSIZE,&tspec);
    } else {
	// do suspend
	ts_is.rc = aio_suspend(list,LISTSIZE,0);
    }
    // maybe remember errno
    if(ts_is.rc) {
	ts_is.err = errno;
    } else {
	ts_is.err = 0;
    }

    // in case we had started a child, wait for it to exit
    // so that next test doesn't get confused
    if(cld>0) {
	int rc = -1;
	int tmp;
	while(rc<0) {
	    rc = wait(&tmp);
	    if(rc<0 && errno!=EINTR) {
		fprintf(stderr,"OOPS: something went wrong, wait() returned %s\n",errnostr(errno));
		exit(-1);
	    }
	}
    }

    // evaluate test
    if(check()) {
	fprintf(stderr,"TEST FAILED\n\n");
    } else {
	fprintf(stderr,"TEST OK\n\n");
    }
    
}



// ////////////////////////////////////////////////////////////////////////////////////
// function check
// Checks and displays the test states "expected"<->"is" (globals).
// params:
//  <void>
// returns:
//  0 values in "is" match the "expected" values
// ////////////////////////////////////////////////////////////////////////////////////
int check() {
    int rc = 0;
    
    fprintf(stderr,"--- rc         : exp %6d is %6d --> ",ts_exp.rc,ts_is.rc);
    if(ts_exp.rc==ts_is.rc) {
	fprintf(stderr,"OK\n");
    } else {
	rc = -1;
	fprintf(stderr,"FAILED\n");
    }
    fprintf(stderr,"--- errno      : exp %-6s is %-6s --> ",errnostr(ts_exp.err),errnostr(ts_is.err));
    if(ts_exp.err==ts_is.err) {
	fprintf(stderr,"OK\n");
    } else {
	rc = -1;
	fprintf(stderr,"FAILED\n");
    }
    fprintf(stderr,"--- num_sigint : exp %6d is %6d --> ",ts_exp.num_sigint,ts_is.num_sigint);
    if(ts_exp.num_sigint==ts_is.num_sigint) {
	fprintf(stderr,"OK\n");
    } else {
	rc = -1;
	fprintf(stderr,"FAILED\n");
    }
    fprintf(stderr,"--- num_sigalrm: exp %6d is %6d --> ",ts_exp.num_sigalrm,ts_is.num_sigalrm);
    if(ts_exp.num_sigalrm==ts_is.num_sigalrm) {
	fprintf(stderr,"OK\n");
    } else {
	rc = -1;
	fprintf(stderr,"FAILED\n");
    }
    
    return rc;
}



// ////////////////////////////////////////////////////////////////////////////////////
// function errnostr
// Returns readable string for an error code.
// params:
//  err    error code as in errno
// returns:
//  string corresponding to err
// ////////////////////////////////////////////////////////////////////////////////////
const char *errnostr(int err) {
    static char str[50];
    switch(err) {
        case EINTR:
	    return "EINTR";
        case EAGAIN:
	    return "EAGAIN";
        case EINVAL:
	    return "EINVAL";
        case ECHILD:
	    return "ECHILD";
	case 0:
	    return "<NULL>";
        default:
	{
	    sprintf(str,"<err=%d>",err);
	    return str;
	}
    }
}


// eof ////////////////////////////////////////////////////////////////////////////////

