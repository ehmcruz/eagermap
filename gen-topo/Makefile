CC=gcc
CPP=g++
AR=ar
CFLAGS=-O2 $(shell pkg-config --cflags hwloc) -ggdb -Wall
CPPFLAGS=$(CFLAGS)
LDFLAGS=$(shell pkg-config --libs hwloc)

binfile=gen-topology

csrcfiles=hwloc-gen-topology.c
headerfiles=$(wildcard *.h)

objfiles=$(patsubst %.c,%.o,$(csrcfiles))

%.o: %.c $(headerfiles)
	$(CC) -c $(CFLAGS) $< -o $@

%.o: %.cpp $(headerfiles)
	$(CPP) -c $(CPPFLAGS) $< -o $@

all: $(binfile)
	@echo "Compiled! Yes!"

$(binfile): $(objfiles)
	$(CC) -o $(binfile) $(objfiles) $(LDFLAGS)

clean:
	- rm -f *.o
	- rm -f $(binfile)

