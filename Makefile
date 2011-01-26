#
# GNU Makefile for wpakey-editor
#
# Copyright (C) 2010 Shaun Amott <shaun@inerd.com>
# All rights reserved.
#
# $Id: Makefile,v 1.2 2010/06/15 20:44:25 samott Exp $
#

APPNAME=	wpakey-editor
VERSION=	0.9
DESTDIR=	# empty

CC=		gcc
PC=		pkg-config
LIBRARIES=	gconf-2.0 hildon-1 gtk+-2.0 libosso
CFLAGS=		-Wall -pedantic `pkg-config --cflags  $(LIBRARIES)`
LDFLAGS=	`pkg-config --libs $(LIBRARIES)`

ifdef DEBUG
CFLAGS+=	-g
LDFLAGS+=	-g
endif

BIN_SCRIPTS=	wpakey
SRC_FILES=	$(APPNAME).c
LIB_FILES=	$(APPNAME).so
ICON_FILES=	data/$(APPNAME).png
DESKTOP_FILES=	data/$(APPNAME).desktop

BIN_DIR=	$(DESTDIR)`$(PC) osso-af-settings --variable=prefix`/bin
ICON_DIR=	$(DESTDIR)`$(PC) libhildondesktop-1 --variable=prefix`/share/icons/hicolor
DESKTOP_DIR=	$(DESTDIR)`$(PC) osso-af-settings --variable=desktopentrydir`
CP_LIB_DIR=	$(DESTDIR)`$(PC) hildon-control-panel --variable=pluginlibdir`
CP_DESKTOP_DIR=	$(DESTDIR)`$(PC) hildon-control-panel --variable=plugindesktopentrydir`

all: $(LIB_FILES) $(BIN_SCRIPTS)

$(LIB_FILES): %.so: %.c
	$(CC) -shared  -Wall $(CFLAGS) $(LDFLAGS) -o $@ $<

wpakey:
	cp wpakey.pl wpakey
	chmod ugo+x wpakey

install:
	mkdir -p $(BIN_DIR)
	mkdir -p $(CP_LIB_DIR)
	mkdir -p $(CP_DESKTOP_DIR)
	mkdir -p $(ICON_DIR)/scalable/hildon
	cp $(BIN_SCRIPTS) $(BIN_DIR)
	cp $(LIB_FILES) $(CP_LIB_DIR)
	cp $(DESKTOP_FILES) $(CP_DESKTOP_DIR)
	cp $(ICON_FILES) $(ICON_DIR)/scalable/hildon

clean:
	rm -f $(LIB_FILES)
	rm -f wpakey

dist: clean
	cd ..						\
	  && cp -a $(CURDIR) $(APPNAME)-$(VERSION)	\
	  && tar czf $(APPNAME)-$(VERSION).tar.gz	\
	    $(APPNAME)-$(VERSION)			\
	  && rm -rf $(APPNAME)-$(VERSION)

# ex: ts=8 sw=8
