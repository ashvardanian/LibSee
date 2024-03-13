# Contributing to LibSee

To compile the library, you will need to have `gcc` and `make` installed. You can install them on Ubuntu with:

```bash
sudo apt-get install build-essential
```

To compile the library, run:

```bash
make
```

Want to try it out? Here's how to use it:

```bash
LD_PRELOAD="$(pwd)/libsee.so" ls        # On Linux
LD_PRELOAD="$(pwd)/libsee.dylib" ls     # On MacOS
```

Debugging:

```bash
make clean && make debug && gdb --args env LD_PRELOAD=$(pwd)/libsee.so ls
```
