CXX = g++
CFLAGS = -g -Wall

BINS = http_server console.cgi

all: http_server console

http_server: http_server.cpp
	$(CXX) $(CFLAGS) -o $@ $^

console: console.cpp
	$(CXX) $(CFLAGS) -o console.cgi $^

debug: CFLAGS += -D DEBUG
debug: $(BINS)

clean:
	rm $(BINS)