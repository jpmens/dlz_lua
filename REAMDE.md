# dlz_lua

This is **dlz_lua**, a dlopen() back-end to BIND, tested with 9.9.0b2,
though it ought to work with any version which supports dlopen().

Steps:

1. Compile the driver, which ought to create a `dlz_lua.so`.
2. Add the driver to your `named.conf` using the sample provided.
3. Restart BIND and cross your fingers. :)

## Examples

The Lua script `example.lua` contains a `lookup` function which
returns a few "special" answers. (Thank you to the people I swiped
some of this code from. ;-)

### Timestamp

	$ dig +noall +answer time.example.nil any
	time.example.nil.  0  IN  TXT  "2011-12-01T10:41:48+0100

### Random password generator

	$ dig +noall +answer password.example.nil txt
	password.example.nil.  0  IN  TXT  "$lf#k4$?72i1X7M"

## Help me

I'm anything but a Lua expert, so I need your help. 

* For example, the bindlog function I define in C, but which gets
  called from Lua, I want to pass a "state" pointer from C into Lua
  and back into C. Currently, bindlog() prints to stderr, but I need
  to be able to syslog() it via named, for which I need this state.
  How can I do that?

Files in this directory:

* `dlz_minimal.h` is a copy from BIND's `contrib/dlz/example`.
