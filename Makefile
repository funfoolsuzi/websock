main: websock.hpp websock.cpp main.cpp
	g++ main.cpp websock.cpp -std=c++11 -pthread -o main -Wall
