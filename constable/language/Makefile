
# $Id: Makefile,v 1.3 2004/04/24 15:33:36 marek Exp $

OBJECTS = conf_lang.o language.o expression.o data.o variables.o lex.o \
		execute.o execute_stack.o alu.o alu2.o constant.o error.o \
		cmds.o types.o

all:	module.o

remake:	clean all

osdep.h:	make_osdep.sh
	./make_osdep.sh >$@

module.o:	osdep.h $(OBJECTS)
	$(LD) -r -o $@ $(OBJECTS)

%.o:	%.c
	$(CC) $(CFLAGS) -ggdb -c $<

clean:
	rm -f *.o

mrproper:
	rm -f *.o osdep.h

