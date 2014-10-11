
# $Id: Makefile,v 1.2 2002/10/23 10:25:43 marek Exp $

OBJECTS = users.o roles.o load.o save.o adm.o adm_perm.o kobjects.o proc.o

all:	module.o

remake:	clean all

module.o:	$(OBJECTS)
	$(LD) -r -o $@ $(OBJECTS)

%.o:	%.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o

mrproper:
	rm -f *.o

