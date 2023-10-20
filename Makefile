OBJ := luaw/luaw.o
CPPFLAGS := -I. -std=c++20 -Wall -Wextra

all: libluaw-54.a libluaw-jit.a

# libluaw-54.a

lua/liblua.a:
	$(MAKE) -C lua MYLDFLAGS= CWARNGCC=

luaw/luaw-54.o: luaw/luaw.cc
	$(CXX) -DLUAW=54 -c ${CPPFLAGS} -Ilua -o $@ $<

libluaw-54.a: luaw/luaw-54.o lua/liblua.a
	ar -rc $@ $<

check-54: luaw/tests.cc libluaw-54.a
	$(CXX) ${CPPFLAGS} -Ilua -DLUAW=54 -o check-54 $<
	./check-54

# libluaw-jit.a

luajit/src/libluajit.a:
	$(MAKE) -C luajit MACOSX_DEPLOYMENT_TARGET=11.7.10

luaw/luaw-jit.o: luaw/luaw.cc
	$(CXX) -DLUAW=JIT -Iluajit/src -o $@ $< ${CPPFLAGS}

libluaw-jit.a: ${OBJ} luajit/src/libluajit.a
	ar -rc $@ $<

check-jit: luaw/tests.cc libluaw-jit.a
	$(CXX) ${CPPFLAGS} -Iluajit/src -DLUAW=JIT -o check-jit $<
	./check-jit

check: check-54 check-jit

clean:
	$(MAKE) -C lua clean
	$(MAKE) -C luajit clean MACOSX_DEPLOYMENT_TARGET=11.7.10
	rm -f *.o libluaw-54.a lubluaw-jit.a check-54 check-jit