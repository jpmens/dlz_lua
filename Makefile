# for building the dlz_lua driver we don't use
# the bind9 build structure as the aim is to provide an
# example that is separable from the bind9 source tree

# this means this Makefile is not portable, so the testsuite
# skips this test on platforms where it doesn't build

LUA=/usr/local/stow/lua-514
CFLAGS=-fPIC -g -Wall -I$(LUA)/include

all: dlz_lua.so

dlz_lua.so: dlz_lua.o
	$(CC) $(CFLAGS) -shared -o dlz_lua.so dlz_lua.o -L$(LUA) -llua

clean:
	rm -f dlz_lua.o dlz_lua.so 
