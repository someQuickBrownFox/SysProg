# usage:
#	make
#		builds the lib (libaio.a) only
#	make all
#		build the lib and all test programs
#		if they have been added below
#	make clean
#		cleans up lib and all test programs
#		if they have been added below
#		(is actually a "clean all")

CFLAGS=-g -Wall -Wextra -Werror -DDEBUG -fno-stack-protector
LDFLAGS=

# all object FOR THE LIB have to be added here
# (test programs etc belong somewhere else, I suggest
# a sub dir)
LIBOBJS=	aio_read.o	\
		    aio_init.o \
		    aio_write.o	\
		    lio_listio.o	\
		    aio_return.o	\
		    aio_error.o	\
		    aio_suspend.o      \
			aio_util.o \
		    aio_cancel.o 
	
SRVOBJS=	aiosrv.o \
			aio_util.o

LIBAIO=libaio.a
AIOSRV=aiosrv

$(LIBAIO): $(AIOSRV) $(LIBOBJS)
	ar -r $(LIBAIO) $(LIBOBJS)

$(AIOSRV): $(SRVOBJS)
	gcc -o $(AIOSRV) $(SRVOBJS)

# add every test programm below as a new line
clean:
	rm -f $(LIBOBJS) $(LIBAIO) $(SRVOBJS) $(AIOSRV)
	make -Ctestprog-sample clean

# add every test programm below as a new line
all: $(LIBAIO)
	make -Ctestprog-sample
