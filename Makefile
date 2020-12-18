CXX=clang++
CXXFLAGS=-std=c++2a -stdlib=libc++ -fimplicit-modules -fimplicit-module-maps \
	-fprebuilt-module-path=src/ -fsanitize=undefined -O0 -g

LD=clang++
LFLAGS=-fsanitize=undefined -O0 -g

LIBS=`pkg-config --libs sfml-all` -lpthread
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

