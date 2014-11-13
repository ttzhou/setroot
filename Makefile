PREFIX   := usr/local
MANDIR   := ${PREFIX}/share/man/man1
BINDIR   := ${PREFIX}/bin

NAME     := setroot
CC       := gcc
OFLAG    := -O0
CFLAGS   := -std=c99 ${OFLAG} -Wall -Wextra -g -pedantic
INCLUDES := -I /usr/include/X11
LIBS     := -L /usr/X11/lib -lX11 -lXinerama `imlib2-config --libs`

SRC      := setroot.c

all:
	${CC} ${CFLAGS} ${SRC} ${INCLUDES} ${LIBS} -o ${NAME}

install: all
	mkdir -p         ${DESTDIR}/${BINDIR}
	mkdir -p         ${DESTDIR}/${MANDIR}
	cp ${NAME}       ${DESTDIR}/${BINDIR}
	cp man/${NAME}.1 ${DESTDIR}/${MANDIR}

uninstall:
	rm -rf -i ${DESTDIR}/${BINDIR}/${NAME}
	rm -rf -i ${DESTDIR}/${BINDIR}/${NAME}.1

clean:
	rm -f ${NAME}
