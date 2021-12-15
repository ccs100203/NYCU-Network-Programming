CXX=g++
CXXFLAGS=-std=c++11 -Wall -pedantic -pthread -lboost_system
CXX_INCLUDE_DIRS=/usr/local/include
CXX_INCLUDE_PARAMS=$(addprefix -I , $(CXX_INCLUDE_DIRS))
CXX_LIB_DIRS=/usr/local/lib
CXX_LIB_PARAMS=$(addprefix -L , $(CXX_LIB_DIRS))

BINS = socks_server hw4.cgi

all: console socks_server

socks_server: socks_server.cpp
	$(CXX) $^ -o $@ $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) $(CXXFLAGS)

console: console.cpp
	# $(CXX) $^ -o hw4.cgi $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) $(CXXFLAGS)
	$(CXX) $^ -o ~/public_html/hw4.cgi $(CXX_INCLUDE_PARAMS) $(CXX_LIB_PARAMS) $(CXXFLAGS)

# check: socks_server console
# 	./socks_server 16795

debug: CFLAGS += -D DEBUG
debug: $(BINS)

clean:
	rm $(BINS)