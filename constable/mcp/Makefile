
# $Id: Makefile,v 1.2 2002/10/23 10:25:43 marek Exp $

OBJECTS = mcp.o conf_lang.o

all:	module.o

remake:	clean all

module.o:	$(OBJECTS)
	$(LD) -r -o $@ $(OBJECTS)

%.o:	%.c
	$(CC) $(CFLAGS) -ggdb -c $<

clean:
	rm -f *.o

mrproper:
	rm -f *.o

