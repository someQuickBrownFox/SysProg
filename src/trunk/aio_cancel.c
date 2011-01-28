#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include "aio.h"


int aio_cancel_once(struct aiocb *aiocb);

extern struct aiocb *HeadPtr;

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
	 struct aiocb *iterationPtr = HeadPtr;		
	 while(iterationPtr != NULL)
	 {
	    if(fildes == iterationPtr->aio_fildes)
			if( aio_cancel_once(iterationPtr) == AIO_ALLDONE)
				once_done = 1;

		iterationPtr = iterationPtr->aio_next;
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
  return -1;
}

int main (void)
{
   aio_cancel(0, NULL); //dummy
   printf("hallo");
   return 0;
}


