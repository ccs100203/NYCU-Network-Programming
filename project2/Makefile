CC = gcc
CFLAGS = -g -Wall -pthread -lrt

BINS = np_simple np_single_proc np_multi_proc

all: $(BINS)
	# clang-format -i *.c *.h

np_simple: server1/np_simple.c server1/npshell.c
	$(CC) $(CFLAGS) -o $@ $^

np_single_proc: server2/np_single_proc.c server2/npshell.c
	$(CC) $(CFLAGS) -o $@ $^

np_multi_proc: server3/np_multi_proc.c server3/npshell.c
	$(CC) $(CFLAGS) -o $@ $^

debug: CFLAGS += -D DEBUG
debug: $(BINS)

clean:
	rm $(BINS)