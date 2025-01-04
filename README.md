![LibSee Thumbnail](https://github.com/ashvardanian/ashvardanian/blob/master/repositories/LibSee.jpg?raw=true)

> _See where you use LibC the most._ <br/>
> _Trace calls failing tests. Then - roast!_

__LibSee__ is a single-file library for profiling LibC calls and 🔜 fuzzy testing.
To download and compile the script and run your favorite query:

```bash
gcc -g -O2 -fno-builtin -fPIC -nostdlib -nostartfiles -shared -o libsee.so libsee.c
```

LibSee overrides LibC symbols using `LD_PRELOAD`, profiling the most commonly used functions, and, optionally, fuzzing their behavior for testing.
The library yields a few binaries when compiled:

```bash
libsee.so # Profiles LibC calls
libsee_and_knee.so # Correct LibC behavior, but fuzzed!
```

## Tricks Used

There are several things worth knowing, that came handy implementing this.

- One way to implement this library would be to override the `_start` symbols, but implementing correct loading sequence for a binary is tricky, so I use conventional `dlsym` to lookup the symbols on first function invocation.
- On `x86_64` architecture, the `rdtscp` instruction yields both the CPU cycle and also the unique identifier of the core. Very handy if you are profiling a multi-threaded application.
- Once the unloading sequence reaches `libsee.so`, the `STDOUT` is already closed. So if you want to print to the console, you may want to reopen the `/dev/tty` device before printing usage stats.
- Calling convention for system calls on Aarch64 and x86 differs significantly. On Aarch64 I use the [generalized `openat`](https://github.com/torvalds/linux/blob/bf3a69c6861ff4dc7892d895c87074af7bc1c400/include/uapi/asm-generic/unistd.h#L158-L159) with opcode 56. On [x86 it's opcode 2](https://github.com/torvalds/linux/blob/0dd3ee31125508cd67f7e7172247f05b7fd1753a/arch/x86/entry/syscalls/syscall_64.tbl#L13).
- On MacOS the `sprintf`, `vsprintf`, `snprintf`, `vsnprintf` are macros. You have to `#undef` them.
- On `Release` builds compilers love replacing your code with `memset` and `memcpy` calls. As the symbol can't be found from inside LibSee, it will `SEGFAULT` so don't forget to disable such optimizations for built-ins `-fno-builtin`.
- No symbol versioning is implemented, vanilla `dlsym` is used over the `dlvsym`.

## Coverage

LibC standard is surprisingly long, so not all of the functions are covered.
Feel free to suggest PRs covering the rest:

- [x] [memory management](https://en.cppreference.com/w/c/memory)
- [x] [byte-strings](https://en.cppreference.com/w/c/string/byte)
- [x] [algorithms](https://en.cppreference.com/w/c/algorithm)
- [x] [date and time](https://en.cppreference.com/w/c/chrono)
- [x] [input/output](https://en.cppreference.com/w/c/io)
- [x] [wide-character strings](https://en.cppreference.com/w/c/string/wide)
- [ ] [concurrency and atomics](https://en.cppreference.com/w/c/thread)
- [ ] retrieving error numbers
- [ ] [numerics](https://en.cppreference.com/w/c/numeric)
- [ ] [multi-byte strings](https://en.cppreference.com/w/c/string/multibyte)
- [ ] [wide-character IO](https://en.cppreference.com/w/c/io)
- [ ] [localization](https://en.cppreference.com/w/c/locale)
- [ ] anything newer than C 11

There are a few other C libraries that most of the world reuses, rather than implementing from scratch in other languages:

- [ ] BLAS and LAPACK
- [ ] PCRE RegEx
- [ ] `hsearch`, `tsearch`, and pattern matching [extensions](https://ftp.gnu.org/old-gnu/Manuals/glibc-2.2.3/html_node/libc_toc.html)

[Program support](https://en.cppreference.com/w/c/program) utilities aren't intended.

