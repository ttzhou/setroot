PREFIX           ?= /usr/local
DESTDIR          ?=
MANDIR           ?= ${PREFIX}/share/man/man1
BINDIR           ?= ${PREFIX}/bin

NAME             ?= setroot
CC               ?= gcc
PKG_CONFIG       ?= pkg-config
OFLAG            ?=
CFLAGS           ?= -std=c99 -Wall -g -Wextra ${OFLAG}

CFLAGS+=$(shell  $(PKG_CONFIG) imlib2 --cflags)
LDFLAGS+=$(shell $(PKG_CONFIG) imlib2 --libs)

CFLAGS+=$(shell  $(PKG_CONFIG) x11 --cflags)
LDFLAGS+=$(shell $(PKG_CONFIG) x11 --libs)

SRC              := setroot.c

ifeq             (${xinerama},1)
DEFINES          += -DHAVE_LIBXINERAMA
CFLAGS+=$(shell  $(PKG_CONFIG) xinerama --cflags)
LDFLAGS+=$(shell $(PKG_CONFIG) xinerama --libs)
endif

all:
	${CC} ${SRC} ${CFLAGS} ${LDFLAGS} ${DEFINES} -o ${NAME}

install: ${NAME} man/${NAME}.1
	mkdir -p         ${DESTDIR}${BINDIR}
	mkdir -p         ${DESTDIR}${MANDIR}
	cp ${NAME}       ${DESTDIR}${BINDIR}
	cp man/${NAME}.1 ${DESTDIR}${MANDIR}

uninstall:
	rm -rf -i ${DESTDIR}${BINDIR}/${NAME}
	rm -rf -i ${DESTDIR}${MANDIR}/${NAME}.1

clean:
	rm -f ${NAME}
