CC = gcc
CXX = g++
CFLAGS = -g -Wall

BINS = npshell

all: $(BINS)
	clang-format -i npshell.*

npshell: npshell.c
	$(CC) $(CFLAGS) -o $@ $<

check: all
	./npshell < script.sh > out.txt 2>&1

debug: CFLAGS += -D DEBUG
debug: npshell all

clean:
	rm $(BINS) test*.txt