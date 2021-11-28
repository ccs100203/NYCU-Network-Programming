CXX=g++
CXXFLAGS=-std=c++11 -Wall -pedantic -pthread -lboost_system
CXX_INCLUDE_DIRS=/usr/local/include
CXX_INCLUDE_PARAMS=$(addprefix -I , $(CXX_INCLUDE_DIRS))
CXX_LIB_DIRS=/usr/local/lib
CXX_LIB_PARAMS=$(addprefix -L , $(CXX_LIB_DIRS))

BINS = http_server console.cgi

all: http_server console

http_server: http_server.cpp
	$(CXX) $^ -o $@ $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) $(CXXFLAGS)

console: console.cpp
	$(CXX) $(CFLAGS) -o console.cgi $^

check: http_server
	./http_server 16795

debug: CFLAGS += -D DEBUG
debug: $(BINS)

clean:
	rm $(BINS)