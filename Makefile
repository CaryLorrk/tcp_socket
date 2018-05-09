all: main

CXXFLAGS=-std=c++14 -Wall -Wextra -g -Og -fno-omit-frame-pointer -pthread

main: main.cc comm.h comm.cc
	$(CXX) $(CXXFLAGS) -o $@ $^

.PHONY: clean

clean:
	rm -rf main
