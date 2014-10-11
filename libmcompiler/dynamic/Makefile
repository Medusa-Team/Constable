
# $Id: Makefile,v 1.2 2002/10/23 10:25:43 marek Exp $

THIS = objects.a
INCDIR = ..

OBJECTS = dynamic.o argv.o to_argv.o llist.o ddata.o dbufers.o dfield.o

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

