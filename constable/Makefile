
# $Id: Makefile,v 1.7 2004/04/24 15:29:53 marek Exp $

##### Edit this - BEGIN #####
PREFIX = 
DESTDIR = $(PREFIX)
BINDIR = $(DESTDIR)/bin
CC = gcc
#CC = clang
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -pedantic-errors -g -DRBAC \
	-I$(CURDIR)/../libmcompiler -D_GNU_SOURCE
LD = ld
#LDFLAGS = ../libmcompiler/libmcompiler.a -lefence
LDFLAGS = ../libmcompiler/libmcompiler.a -pthread
#LDFLAGS = -lmcompiler -lefence
#LDFLAGS = -lmcompiler 
AR = ar
MAKE = make
# MAKE = gmake
CP = cp
##### Edit this - END #####

# Do not touch rest of the file!

OBJECTS = vs.o tree.o space.o class.o object.o object_endian.o decision.o event.o \
	  comm.o comm_buf.o hash.o constable.o init.o generic.o string_utils.o \
	  access_types.o policy_inspect.o
MODULES = language mcp rbac
ADDON = Mlibc

.PHONY: test

all:	constable rbacadm

test: constable
	$(MAKE) -C tests CC="$(CC)" CFLAGS="$(CFLAGS)"

remake:	clean all

constable:	$(OBJECTS) modules.a
	$(CC) -o $@ $(OBJECTS) modules.a $(LDFLAGS)
	rm -f modules.a
#	strip $@

rbacadm:	rbacadm.c
	$(CC) $(CFLAGS) -o $@ rbacadm.c

%.o:	%.c
	$(CC) $(CFLAGS) -c $<

#%.o:	%
#	$(MAKE) -C $< module.o CC="$(CC)" CFLAGS="$(CFLAGS)" \
#		LD="$(LD)" AR="$(AR)" MAKE="$(MAKE)"
#	mv $</module.o $@

modules.a:
	$(MAKE) modules TARGET=module.o
	for i in $(MODULES) ; do mv $$i/module.o $$i.o ; $(AR) r $@ $$i.o ; done

modules:
	set -e ; for i in $(MODULES) ; do $(MAKE) -C $$i $(TARGET) \
		CC="$(CC)" CFLAGS="$(CFLAGS)" \
		LD="$(LD)" AR="$(AR)" MAKE="$(MAKE)" ; done

addon:
	set -e ; for i in $(ADDON) ; do $(MAKE) -C $$i $(TARGET) \
		CC="$(CC)" CFLAGS="$(CFLAGS)" \
		LD="$(LD)" AR="$(AR)" MAKE="$(MAKE)" ; done

install: constable rbacadm
	$(CP) constable rbacadm $(BINDIR)

clean:
	rm -f *.o modules.a constable rbacadm
	$(MAKE) modules TARGET=clean
	$(MAKE) addon TARGET=clean
	$(MAKE) -C tests clean

mrproper:
	rm -f *.o modules.a constable rbacadm
	$(MAKE) modules TARGET=mrproper
	$(MAKE) addon TARGET=mrproper
