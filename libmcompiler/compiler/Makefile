
# $Id: Makefile,v 1.2 2002/10/23 10:25:43 marek Exp $

THIS = objects.a
INCDIR = ..
CFLAGS = -Wall -ggdb

OBJECTS = compiler.o synt.o special_out.o lex.o \
	  nul_preprocesor.o gcc_preprocesor.o \
	  f_preprocesor.o \
	  c_lex.o
# c_preprocesor.o

all:	$(THIS)

remake:	clean all

$(THIS):	$(OBJECTS)
	$(AR) r $@ $(OBJECTS)

%.o:	%.c
	$(CC) $(CFLAGS) -I $(INCDIR) -c $<

clean:
	rm -f *.o core $(THIS) test_c_lex test_c_pre

pokus:
	echo $(CFLAGS)

test_c_lex:	test_c_lex.c objects.a
	$(CC) $(CFLAGS) -I $(INCDIR) test_c_lex.c -o test_c_lex objects.a ../text/backslash.c -lm

test_c_pre:	test_c_pre.c objects.a
	$(CC) $(CFLAGS) -I $(INCDIR) test_c_pre.c -o test_c_pre objects.a

