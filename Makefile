CC=gcc
NAME=bmputil
CFILES=bmputil.c
OBJS=${CFILES:.c=.o}
CFLAGS=-Wall -Werror -O2 -std=gnu99
INCLUDES=$(shell pkg-config --cflags 'MagickCore < 7')
LDFLAGS=
LIBS=$(shell pkg-config --libs 'MagickCore < 7')
all: ${NAME}

.PHONY: clean

${OBJS}: %.o: %.c
	${CC} ${CFLAGS} ${INCLUDES} -o $@ -c $<

${NAME}: ${OBJS}
	${CC} ${LDFLAGS} -o $@ $^ ${LIBS}

clean:
	rm -f ${NAME} ${OBJS}
