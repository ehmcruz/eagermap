CC=gcc
CPP=g++
AR=ar
CFLAGS=-O2 -fopenmp -g -Wall
LDFLAGS=

#################################################################

csrcfiles=$(wildcard *.c)
headerfiles=$(wildcard *.h)

#################################################################

objfiles=$(patsubst %.c,%.o,$(csrcfiles))

%.o: %.c $(headerfiles)
	$(CC) -c $(CFLAGS) $< -o $@

#################################################################

all: eagermap

eagermap: $(objfiles)
	gcc -o $@ $(objfiles) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f *.o eagermap maptool
