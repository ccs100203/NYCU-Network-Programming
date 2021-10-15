CC = gcc
CXX = g++
CFLAGS = -Wall

BINS = npshell

all: $(BINS)

npshell: npshell.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm $(BINS)