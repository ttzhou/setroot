PREFIX   := usr/local
MANFIX   := ${PREFIX}/share/man/

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
	mkdir -p         ${DESTDIR}/${PREFIX}
	cp ${NAME}       ${DESTDIR}/${PREFIX}
	cp man/${NAME}.1 ${DESTDIR}/${MANFIX}

uninstall:
	rm -rf -i ${DESTDIR}/${PREFIX}/${NAME}
	rm -rf -i ${DESTDIR}/${MANFIX}/${NAME}.1

clean:
	rm -f ${NAME}
