
# $Id: Makefile,v 1.2 2002/10/23 10:25:43 marek Exp $

NAME = mcompiler
VERSION = 1.0

##### Edit this - BEGIN #####
ROOT = 
DESTDIR = $(ROOT)/usr/local
CC = gcc
#CFLAGS = -Wall -O2
CFLAGS = -Wall -ggdb
LD = ld
LDFLAGS =
AR = ar
MAKE = make
# MAKE = gmake
##### Edit this - END #####

# Do not touch rest of the file!

DESTLIB = $(DESTDIR)/lib
DESTINCLUDE = $(DESTDIR)/include

LIBSTATIC = lib$(NAME).a
LIBDYNAMIC = lib$(NAME).so
LIBDYNAMICV = $(LIBDYNAMIC).$(VERSION)

BUILDDIR = objects
INCDIR = .

SUBDIRS = compiler dynamic other

OBJECTS =  

all:	$(LIBSTATIC) $(LIBDYNAMICV)
	rm -rf $(BUILDDIR)/*
	touch $(LIBSTATIC) $(LIBDYNAMICV)
	@echo All done.
	@echo Type \"make install\" or \"make reinstall\".

remake:	clean all

new:	clean_ar all

$(LIBSTATIC):	$(BUILDDIR) $(OBJECTS)
	$(AR) r $@ $(OBJECTS) $(BUILDDIR)/*
$(LIBDYNAMICV):	$(BUILDDIR) $(OBJECTS)
	$(LD) $(LDFLAGS) -shared -o $@ $(OBJECTS) $(BUILDDIR)/*
%.o:	%.c
	$(CC) $(CFLAGS) -I $(INCDIR) -c $<

subdirs:
	set -e ; for i in $(SUBDIRS) ; do $(MAKE) -C $$i $(TARGET) \
		BUILDDIR="../$(BUILDDIR)" \
		CC="$(CC)" CFLAGS="$(CFLAGS)" \
		LD="$(LD)" AR="$(AR)" MAKE="$(MAKE)" ; done

$(BUILDDIR):
	$(MAKE) subdirs TARGET=objects.a
	mkdir $(BUILDDIR)
	set -e ; cd $(BUILDDIR) ; for i in $(SUBDIRS) ; do \
		$(AR) x ../$$i/objects.a ; done


reinstall:	uninstall install

install: $(LIBSTATIC) $(LIBDYNAMICV)
	cp $(LIBSTATIC) $(DESTLIB)/
	cp $(LIBDYNAMICV) $(DESTLIB)/
	ln -s $(LIBDYNAMICV) $(DESTLIB)/$(LIBDYNAMIC)
	mkdir $(DESTINCLUDE)/$(NAME)
	cp $(NAME)/*.h $(DESTINCLUDE)/$(NAME)
uninstall:
	rm -f $(DESTLIB)/$(LIBSTATIC)
	rm -f $(DESTLIB)/$(LIBDYNAMIC)
	rm -f $(DESTLIB)/$(LIBDYNAMICV)
	rm -rf $(DESTINCLUDE)/$(NAME)
clean:
	rm -f *.o core lib$(NAME).*
	rm -rf $(BUILDDIR)
	$(MAKE) subdirs TARGET=clean
clean_ar:
	rm -f core lib$(NAME).*
	rm -rf $(BUILDDIR)
	set -e ; for i in $(SUBDIRS) ; do \
		rm -f $$i/objects.a ; done

remove_objects:
	rm -f *.o core 
	rm -rf $(BUILDDIR)
	$(MAKE) subdirs TARGET=clean

