CC = gcc
CFLAGS = -g -Wall

BINS = np_simple np_single_proc

all: $(BINS)
	# clang-format -i *.c *.h

np_simple: np_simple.c npshell.c
	$(CC) $(CFLAGS) -o $@ $^

np_single_proc: np_single_proc.c
	$(CC) $(CFLAGS) -o $@ $^

check: all
	./np_simple < script.sh > out.txt 2>&1

debug: CFLAGS += -D DEBUG
debug: np_simple all

clean:
	rm $(BINS)