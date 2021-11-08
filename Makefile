CC = gcc
CFLAGS = -g -Wall

BINS = np_simple np_single_proc

all: $(BINS)
	# clang-format -i *.c *.h

np_simple: server1/np_simple.c server1/npshell.c
	$(CC) $(CFLAGS) -o $@ $^

np_single_proc: server2/np_single_proc.c server2/npshell.c
	$(CC) $(CFLAGS) -o $@ $^

debug: CFLAGS += -D DEBUG
debug: np_simple np_single_proc

clean:
	rm $(BINS)