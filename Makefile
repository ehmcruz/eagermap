CC?=gcc
CFLAGS=-O2 -g -Wall
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
	$(CC) -o $@ $(objfiles) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f *.o eagermap
