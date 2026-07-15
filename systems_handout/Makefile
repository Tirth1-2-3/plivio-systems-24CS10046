CXX ?= c++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -pedantic

all: sender receiver

sender: sender.cpp protocol.hpp
	$(CXX) $(CXXFLAGS) -o sender sender.cpp

receiver: receiver.cpp protocol.hpp
	$(CXX) $(CXXFLAGS) -o receiver receiver.cpp

clean:
	rm -f sender receiver
