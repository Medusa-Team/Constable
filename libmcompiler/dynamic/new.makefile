
# $Id: new.makefile,v 1.2 2002/10/23 10:25:44 marek Exp $

VERSION=1.0
ROOT=
DESTDIR= $(ROOT)/usr/local
DESTLIB= $(DESTDIR)/lib
DESTINCLUDE= $(DESTDIR)/include

CC=gcc
CFLAGS= -Wall
LD=ld
LDFLAGS=
AR=ar

all:	libdynamic.a libdynamic.so.$(VERSION)
libdynamic.a:	head.o llist.o ddata.o dbufers.o
	$(AR) r libdynamic.a head.o llist.o ddata.o dbufers.o
libdynamic.so.$(VERSION):	head.o llist.o ddata.o dbufers.o
	$(LD) $(LDFLAGS) -shared -o libdynamic.so.$(VERSION) head.o llist.o ddata.o dbufers.o
head.o:
	$(CC) $(CFLAGS) -c head.c
llist.o:
	$(CC) $(CFLAGS) -c llist.c
ddata.o:
	$(CC) $(CFLAGS) -c ddata.c
dbufers.o:
	$(CC) $(CFLAGS) -c dbufers.c

install: libmarek.a libmarek.so.$(VERSION)
	cp libdynamic.a $(DESTLIB)/
	cp libdynamic.so.$(VERSION) $(DESTLIB)/
	ln -s libdynamic.so.$(VERSION) $(DESTLIB)/libdynamic.so
	cp dynamic.h $(DESTINCLUDE)/
uninstall:
	rm -f $(DESTLIB)/libdynamic.a
	rm -f $(DESTLIB)/libdynamic.so.$(VERSION)
	rm -rf $(DESTINCLUDE)/dynamic.h
clean:
	rm -f *.o core libdynamic.*

