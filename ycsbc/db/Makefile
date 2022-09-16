CC=g++
CFLAGS=-std=c++17 -c -g -o2 -Wall -fgnu-tm -I../../include/
INCLUDES=-I../
SOURCES=$(wildcard *.cc)
OBJECTS=$(SOURCES:.cc=.o)

all: $(SOURCES) $(OBJECTS)

.cc.o:
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@

clean:
	$(RM) $(OBJECTS)

