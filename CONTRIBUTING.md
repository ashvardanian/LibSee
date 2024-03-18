# Contributing to LibSee

## Compiling LibSee

To compile the library, you will need to have any C compiler and CMake installed.
You can install them on Ubuntu with:

```bash
sudo apt-get install cmake build-essential
```

To compile the library, run:

```bash
cmake -B build_release
cmake --build build_release --config Release
test -e build_release/libsee.so && echo "Success" || echo "Failure"
```

Want to try it out? Here's how to use it:

```bash
LD_PRELOAD="$(pwd)/build_release/libsee.so" ls        # On Linux
LD_PRELOAD="$(pwd)/build_release/libsee.dylib" ls     # On MacOS
```

## Testing

Using modern syntax, this is how you build and run the test suite:

```bash
cmake -DCMAKE_BUILD_TYPE=Debug -B build_debug
cmake --build build_debug --config Debug
build_debug/libsee_test
```

To debug:

```bash
LD_PRELOAD=$(pwd)/libsee.so ls # check release build
LD_PRELOAD=$(pwd)/libsee.so ls # check debug build
gdb --args env LD_PRELOAD=$(pwd)/libsee.so ls # debug with gdb
gdb --args env LD_PRELOAD=$(pwd)/libsee.so ls # catch release-only bugs
```
