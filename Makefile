PREFIX   := usr/local
MANDIR   := ${PREFIX}/share/man/man1
BINDIR   := ${PREFIX}/bin

NAME     := setroot
CC       := gcc
OFLAG    := -O2
CFLAGS   := -std=c99 -Wall -Wextra ${OFLAG}
INCLUDES := -I/usr/include/X11/extensions
LIBS     := -lX11 -lXinerama `imlib2-config --libs`

SRC      := setroot.c

all:
	${CC} ${SRC} ${CFLAGS} ${INCLUDES} ${LIBS} -o ${NAME}

install: all
	mkdir -p         ${DESTDIR}/${BINDIR}
	mkdir -p         ${DESTDIR}/${MANDIR}
	cp ${NAME}       ${DESTDIR}/${BINDIR}
	cp man/${NAME}.1 ${DESTDIR}/${MANDIR}

uninstall:
	rm -rf -i ${DESTDIR}/${BINDIR}/${NAME}
	rm -rf -i ${DESTDIR}/${MANDIR}/${NAME}.1

clean:
	rm -f ${NAME}
