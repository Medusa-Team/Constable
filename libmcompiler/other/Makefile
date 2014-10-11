
# $Id: Makefile,v 1.2 2002/10/23 10:25:44 marek Exp $

THIS = objects.a
INCDIR = ..

OBJECTS = backslash.o

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

