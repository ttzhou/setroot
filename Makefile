PREFIX     = /usr/local/bin
MANPREFIX  = $(PREFIX)/share/man

NAME       = setroot
CC         = gcc
OFLAG      = -O3
CFLAGS     = -std=c99 ${OFLAG} -Wall -Wextra -g -pedantic
INCLUDES   = -I /usr/include/X11
LIBS       = -L /usr/X11/lib -lX11 -lXinerama `imlib2-config --libs`

SRC        = setroot.c

all:
	${CC} ${CFLAGS} ${SRC} ${INCLUDES} ${LIBS} -o ${NAME}

install: all
	cp ${NAME} ${PREFIX}

uninstall:
	rm -rf ${PREFIX}/${NAME}

clean:
	rm -f ${NAME}
