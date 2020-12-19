CXX=clang++
CXXFLAGS=-std=c++2a -stdlib=libc++ -fimplicit-modules -fimplicit-module-maps \
	-fprebuilt-module-path=src/ -fsanitize=address -Wall -Wextra -pedantic -O2 -g

LD=clang++
LFLAGS=-fsanitize=address -O2 -g

# I
# FUCKING
# HATE
# C++
# LINKER
# ERRORS
# i had 58000 bytes of linker errors on a program whose entire source
# code is 34942 bytes because -lc++ was missing
LIBS=`pkg-config --libs sfml-all` -lpthread -lc++
OBJS=src/main.o src/game.pcm src/gfx.pcm src/str.pcm
QuakeOats: Makefile $(OBJS)
	$(LD) $(LFLAGS) -o $@ $(OBJS) $(LIBS)

src/main.o: src/main.cc src/game.pcm src/gfx.pcm src/str.pcm
	$(CXX) $(CXXFLAGS) -c -o $@ $<
src/game.pcm: src/game.cc src/gfx.pcm
	$(CXX) $(CXXFLAGS) -c $< -Xclang -emit-module-interface -o $@ 
src/gfx.pcm: src/gfx.cc src/str.pcm
	$(CXX) $(CXXFLAGS) -c $< -Xclang -emit-module-interface -o $@
src/str.pcm: src/str.cc
	$(CXX) $(CXXFLAGS) -c $< -Xclang -emit-module-interface -o $@

.PHONY: clean
clean:
	rm -rf QuakeOats
	find src/ -type f -name "*.o"   -exec rm -rf {} \+
	find src/ -type f -name "*.pcm" -exec rm -rf {} \+

