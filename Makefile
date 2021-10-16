CC = gcc
CXX = g++
CFLAGS = -g -Wall

BINS = npshell

all: $(BINS)
	clang-format -i npshell.*

npshell: npshell.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm $(BINS)