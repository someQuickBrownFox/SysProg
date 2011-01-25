#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include "aio.h"

// Muss noch in aio.h
#define AIO_CANCELED            0x1
#define AIO_NOTCANCELED         0x2
#define AIO_ALLDONE             0x3



struct aiocb *HeadPtr = NULL;

int aio_cancel (int fildes, struct aiocb *aiocb) 
{
	//Check FileDescriptor
	fcntl(fildes, F_GETFD);
	if(errno == EBADF ) //BadFileDescriptor
	{
	  printf("aio_cancel: BadFileDescriptor\n"); //debug
	  return -1;
	}
  
	if (aiocb != NULL)
	{
	  // Treat One	
	  if(fildes == aiocb->aio_fildes)
	  {
	     return aio_cancel_once(aiocb);
	  }
	  else
	  {
	    // OwnSpecification!!!
	    fprintf(stderr, "aio_cancel: fildes doesnt match\n");
	    errno = EINVAL;
	    return -1;
	  }
	}
	else
	{
	 // Treat All
	 int once_done = 0;
	 struct aiocb *laufPtr = HeadPtr;		
	 while(laufPtr != NULL)
	 {
	    if(fildes == laufPtr->aio_fildes)
			if( aio_cancel_once(laufPtr) == AIO_ALLDONE)
				once_done = 1;

		laufPtr = laufPtr->aio_next;
	 }
	 if(once_done == 1)
	  	return AIO_NOTCANCELED;
	}
	return 0;
}

int aio_cancel_once(struct aiocb *aiocb)
{
  int status = 0;

  if( kill(aiocb->aio_pid, SIGTERM) == -1 )
  {
	aiocb->aio_errno=errno;
	return -1;
  }

  if( waitpid(aiocb->aio_pid, &status, WNOHANG) == -1 ) // return immediately if no child hasexited.
  {
	aiocb->aio_errno=errno;
	return -1;
  }

  if (WIFEXITED(status)) 
  {
    printf("aio_cancel: process already finished\n");  //debug
    return AIO_ALLDONE;
  }
  else if (WIFSIGNALED(status)) 
  {
    printf("aio_cancel: process killed by signal\n");  //debug
    aiocb->aio_errno = ECANCELED;
    return AIO_CANCELED;
  }

}

int main (void)
{
   aio_cancel(0, NULL); //dummy
   return 0;
}


