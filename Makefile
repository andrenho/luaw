OBJ := luaw/luaw.o
CPPFLAGS := -std=c++20 -Wall -Wextra

all: libluaw-54.a libluaw-jit.a

lua/liblua.a:
	$(MAKE) -C lua MYLDFLAGS= CWARNGCC=

luajit/src/libluajit.a:
	$(MAKE) -C luajit MACOSX_DEPLOYMENT_TARGET=11.7.10

libluaw-54.a: ${OBJ} lua/liblua.a
	ar -rc $@ $<

libluaw-jit.a: ${OBJ} luajit/src/libluajit.a
	ar -rc $@ $<

check-54: luaw/tests.c libluaw-54.a
	gcc -o tests $< -DLUAW=54
	./tests ; rm ./tests

check-jit: luaw/tests.c libluaw-jit.a
	gcc -o tests $< -DLUAW=JIT
	./tests ; rm ./tests

check: check-54 check-jit

clean:
	$(MAKE) -C lua clean
	$(MAKE) -C luajit clean MACOSX_DEPLOYMENT_TARGET=11.7.10
	rm -f ${OBJ} libluaw-54.a lubluaw-jit.a