.PHONY: test

test: redsea
	sudo ./redsea /dev/nbd0p1

redsea: redsea.cpp redsea.h main.cpp
	g++ -std=c++11 -g -o redsea redsea.cpp main.cpp
