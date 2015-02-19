CC=gcc
CPP=g++
AR=ar
CFLAGS=-O2 -fPIC -ggdb -Wall -fopenmp
LDFLAGS=-lpthread -lm

#################################################################

csrcfiles=$(wildcard *.c)
headerfiles=$(wildcard *.h)

#################################################################

objfiles=$(patsubst %.c,%.o,$(csrcfiles))

%.o: %.c $(headerfiles)
	$(CC) -c $(CFLAGS) $< -o $@

%.o: %.cpp $(headerfiles)
	$(CPP) -c $(CPPFLAGS) $< -o $@

#################################################################

all: $(objfiles)
	gcc -o maptool $(objfiles) $(CFLAGS) $(LDFLAGS)
	@echo "Compiled! Yes!"

clean:
	- rm -f *.o
	- rm -f maptool

