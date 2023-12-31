OBJ := luaw/luaw.o
CPPFLAGS := -I. -std=c++20 -Wall -Wextra `pkg-config --cflags zlib`
LDFLAGS := `pkg-config --libs zlib`

CXX = g++

ifdef DEBUG
	CPPFLAGS += -Og -g3
else
	CPPFLAGS += -Ofast
endif

all: libluaw-54.a libluaw-jit.a luazh-54 luazh-jit

#
# libluaw-54.a
#

lua/liblua.a:
	$(MAKE) -C lua MYLDFLAGS= CWARNGCC=

luaw/luaw-54.o: luaw/luaw.cc lua/liblua.a
	$(CXX) -DLUAW=54 -c ${CPPFLAGS} -Ilua -o $@ $<

libluaw-54.a: luaw/luaw-54.o lua/liblua.a
	mkdir -p tmp54
	cd tmp54 && ar -x ../lua/liblua.a
	ar -rcv $@ $< tmp54/*.o
	rm -rf tmp54

luazh-54: luazh/luazh.cc libluaw-54.a
	$(CXX) ${CPPFLAGS} -Ilua -DLUAW=54 -o $@ $^ ${LDFLAGS} 

check-54: luaw/tests.cc libluaw-54.a luazh-54
	./luazh-54 test luazh/test.lua > luaw/test-54.hh
	$(CXX) ${CPPFLAGS} -Ilua -DLUAW=54 -o $@ luaw/tests.cc libluaw-54.a ${LDFLAGS} 

#
# libluaw-jit.a
#

luajit/src/libluajit.a:
	$(MAKE) -C luajit MACOSX_DEPLOYMENT_TARGET=11.7.10

luaw/luaw-jit.o: luaw/luaw.cc luajit/src/libluajit.a
	$(CXX) -DLUAW=JIT -c ${CPPFLAGS} -Iluajit/src -o $@ $<

libluaw-jit.a: luaw/luaw-jit.o luajit/src/libluajit.a
	mkdir -p tmpjit
	cd tmpjit && ar -x ../luajit/src/libluajit.a
	ar -rcv $@ $< tmpjit/*.o
	rm -rf tmpjit

luazh-jit: luazh/luazh.cc libluaw-jit.a
	$(CXX) ${CPPFLAGS} -Iluajit/src -DLUAW=JIT -o $@ $^ ${LDFLAGS} 

check-jit: luaw/tests.cc libluaw-jit.a luazh-jit
	./luazh-jit test luazh/test.lua > luaw/test-jit.hh
	$(CXX) ${CPPFLAGS} -Iluajit/src -DLUAW=JIT -o $@ luaw/tests.cc libluaw-jit.a ${LDFLAGS} 

#
# other targets
#

check: check-54 check-jit
	./check-54
	./check-jit

clean:
	$(MAKE) -C lua clean
	$(MAKE) -C luajit clean MACOSX_DEPLOYMENT_TARGET=11.7.10
	rm -f *.a *.o luaw/*.o libluaw-54.a lubluaw-jit.a check-54 check-jit luaw/test-*.hh luazh-jit luazh-54
