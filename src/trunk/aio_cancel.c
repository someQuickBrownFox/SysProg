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
	 int once_canceled = 0;
	 struct aiocb *iterationPtr = HeadPtr;	
	 do
	 {
	    if(fildes == iterationPtr->aio_fildes)
	    {
			int backValue = aio_cancel_once(iterationPtr);
			if(backValue == AIO_ALLDONE)
			{
				once_done = 1;
			}
			else if (backValue == AIO_CANCELED)
			{
			      once_canceled = 1;
			}
	    }
	    
	    iterationPtr = iterationPtr->aio_next;
	 }
	 while (iterationPtr != NULL);
	 if(once_done == 1 && once_canceled == 1)
	 {
	  	return AIO_NOTCANCELED;
	}
	else if (once_done == 0 && once_canceled == 1)
	{
	    return AIO_CANCELED;
	}
	else if (once_canceled == 0 && once_done == 1)
	{
	  return AIO_ALLDONE;
	}
	return 0;
}
}

int aio_cancel_once(struct aiocb *aiocb)
{
  int status = 0;
 
  if( kill(aiocb->aio_pid, SIGKILL) == -1 )
  {
	aiocb->aio_errno=errno;
	return -1;
  }

  if( waitpid(aiocb->aio_pid, &status, 0) == -1 ) // return immediately if no child hasexited.
  {
	aiocb->aio_errno=errno;
	return -1;
  }

  if (WIFEXITED(status)) 
  {
    return AIO_ALLDONE;
  }
  else if (WIFSIGNALED(status)) 
  {
    //Process killed by signal
    aiocb->aio_errno = ECANCELED;
    return AIO_CANCELED;
  }
  return -1;
}


