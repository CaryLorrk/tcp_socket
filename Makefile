all: socket.out grpc.out

CXX = g++
CXXFLAGS = -std=c++14 -Wall -Wextra -g -Og -fno-omit-frame-pointer -pthread
LDFLAGS := $(shell pkg-config --libs grpc++) \
	$(shell pkg-config --libs protobuf)
	
PROTOC = protoc
GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= `which $(GRPC_CPP_PLUGIN)`

socket.out: main.cc socket/comm.h socket/comm.cc socket/serialize.h
	$(CXX) $(CXXFLAGS) $^ -o $@

grpc.out: main.cc grpc/comm.h grpc/comm.cc grpc/serialize.h grpc/comm_server.h grpc/comm_server.cc grpc/comm.grpc.pb.cc grpc/comm.pb.cc
	$(CXX) -D GRPC $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

%.grpc.pb.cc: %.proto
	$(PROTOC) -I $(shell dirname $@) --grpc_out=$(shell dirname $@) --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $<

%.pb.cc: %.proto
	$(PROTOC) -I $(shell dirname $@) --cpp_out=$(shell dirname $@) $<

.PHONY: clean

clean:
	rm -rf socket.out grpc.out
