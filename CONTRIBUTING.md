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
LD_PRELOAD=/home/ubuntu/LibSeeProfiler/libsee.so ls
```

Debugging:

```bash
make debug && gdb --args env LD_PRELOAD=/home/ubuntu/LibSeeProfiler/libsee.so ls

```
