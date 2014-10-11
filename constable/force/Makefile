
# $Id: Makefile,v 1.2 2002/10/23 10:25:43 marek Exp $

OBJECTS = elf.o linker.o force.o dloader.o dloader_old.o

all:	module.o

remake:	clean all

module.o:	$(OBJECTS)
	$(LD) -r -o $@ $(OBJECTS)

%.o:	%.c
	$(CC) $(CFLAGS) -c $<

%.o:	%.S
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o

mrproper:
	rm -f *.o

