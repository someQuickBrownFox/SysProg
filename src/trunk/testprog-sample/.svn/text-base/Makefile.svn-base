# This is an example for a test program
#
# TODO: only set a correct OBJS and TARGET, that's it
#
# Here only one own source file is used (main.c)
# If there are more, the OBJS has to look like:
# OBJS=src1.c src2.c src3.c
#  or
# OBJS=	src1.c \
#	src2.c \
#	src3.c
# NB: from the second line there should be a <tab>
#

CFLAGS=-g -Wall -Wextra -Werror

OBJS=main.o
TARGET=testprog-sample

all: ../libaio.a $(OBJS)
	gcc -o $(TARGET) $(OBJS) ../libaio.a

clean:
	rm -f $(OBJS) $(TARGET)

../libaio.a:
	make -C.. libaio.a
