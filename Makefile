CXX=clang++
CXXFLAGS=-std=c++2a -stdlib=libc++ -fimplicit-modules -fimplicit-module-maps \
	-fprebuilt-module-path=src/ -fsanitize=address -Wall -Wextra -pedantic -O2 -g

LD=clang++
LFLAGS=-fsanitize=address -O2 -g

LIBS=$(shell pkg-config --libs sfml-all) -lpthread
OBJS=src/main.o
QuakeOats: Makefile $(OBJS)
	$(LD) $(LFLAGS) -o $@ $(OBJS) $(LIBS)

src/main.o: src/main.cc src/game.hpp src/gfx.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

all: QuakeOats

.PHONY: clean
clean:
	rm -rf QuakeOats
	find src/ -type f -name "*.o"   -exec rm -rf {} \+
	find src/ -type f -name "*.pcm" -exec rm -rf {} \+

