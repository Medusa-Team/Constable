
# $Id: Makefile,v 1.2 2002/10/23 10:25:43 marek Exp $

THIS = objects.a
INCDIR = ..
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -pedantic-errors -fPIC \
	-D_GNU_SOURCE
DEBUG ?= 0
ifeq ($(DEBUG),1)
override CFLAGS += -g
endif

OBJECTS = compiler.o synt.o special_out.o lex.o \
	  nul_preprocesor.o gcc_preprocesor.o \
	  f_preprocesor.o \
	  c_lex.o

all:	$(THIS)

remake:	clean all

$(THIS):	$(OBJECTS)
	$(AR) r $@ $(OBJECTS)

%.o:	%.c
	$(CC) $(CFLAGS) -I $(INCDIR) -c $<

clean:
	rm -f *.o core $(THIS)

pokus:
	echo $(CFLAGS)
