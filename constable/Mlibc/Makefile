# Makefile for Mlibc, (C) 2000 Medusa Development Team
# $Id: Makefile,v 1.2 2002/10/23 10:25:43 marek Exp $

CC= gcc
CCOPTS= -Wall -O2
LDOPTS=
AS= as
ASOPTS=

F_OBJS = f_exit.mo f_getpeername.mo f_secure_unlink.mo f_showargs.mo f_test.mo f_test2.mo
MLIBC_OBJS = mlibc.o


all:	$(F_OBJS)

%.o:	%.c
	$(CC) $(CCOPTS) -c -o $*.o $<

%.mo:	%.o $(MLIBC_OBJS)
	$(LD) $(LDOPTS) -r -o $*.mo $< $(MLIBC_OBJS)

.S.o:
	$(AS) $(ASOPTS) -o $*.o $<

clean:
	-rm -f $(F_OBJS) *.o

mrproper:
	-rm -f $(F_OBJS) *.o

