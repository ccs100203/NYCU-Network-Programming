CC = gcc
CXX = g++
CFLAGS = -Wall -g

BINS = npshell

all: $(BINS)
	clang-format -i npshell.*

npshell: npshell.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm $(BINS)