# LibSee

> _See where you use LibC the most._ <br/>
> _Trace calls failing tests. Then - roast!_

One-liner, to download and compile the script and run your vavorite query:

```bash
ls
```

LibSee overrides LibC symbols using `LD_PRELOAD`, profiling the most commonly used functions, and, optionally, fuzzing their behaviour for testing.
The library yields a few binaries when compiled:

```bash
libsee.so # Profiles LibC calls
libsee_fuzz.so # Correct LibC behaviour, but fuzzed!
```

Alternatively, you can generate profilers for a provided list of symbols using Python script:

```bash
$ pip install libsee
$ libsee --symbols malloc,free ls
```

## Profiling Examples

I've provided several profiling logs for different binaries in Gists:

- Linux utilities: [ls](https://gist.github.com/ashvardanian/libsee-ls.log), [cat](https://gist.github.com/ashvardanian/libsee-cat.log), [echo](https://gist.github.com/ashvardanian/libsee-echo.log)
- Common Pyhon applications: FastAPI server.
- Databses and Search: ClickHouse, USearch.

## Fuzzing Functionality

Fuzzing only covers a few headers and functions for now:

## Tricks Used

There are several things worth knowing, that came handy implementing this.

- One way to implement this library would be to override the `_start` symbols, but implementing correct loading sequence for a binary is tricky, so I use conventional `dlsym` to lookup the symbols on first function invocation.
- On `x86_64` architecture, the `rdtscp` instruction yields both the CPU cycle and also the unique identifier of the core. Very handy if you are profiling a multi-threaded application.
- Once the unloading sequence reaches `libsee.so`, the `STDOUT` is already closed. So if you want to print to the console, you may want to reopen the `/dev/tty` device before printing usage stats.
- On MacOS the `sprintf`, `vsprintf`, `snprintf`, `vsnprintf` are macros. You have to `#undef` them.
- On `Release` builds compilers love replacing your code with `memset` and `memcpy` calls. As the symbol can't be found from inside LibSee, it will `SEGFAULT` so don't forget to disable such optimizations for built-ins `-fno-builtin`.

## Coverage

LibC standard is surprisingly long, so not all of the functions are covered.
Feel free to suggest PRs covering the rest:

- [x] [memory management](https://en.cppreference.com/w/c/memory)
- [x] [byte-strings](https://en.cppreference.com/w/c/string/byte)
- [x] [algorithms](https://en.cppreference.com/w/c/algorithm)
- [x] [date and time](https://en.cppreference.com/w/c/chrono)
- [x] [input/output](https://en.cppreference.com/w/c/io)
- [ ] [concurrency and atomics](https://en.cppreference.com/w/c/thread)
- [ ] retrieving error numbers
- [ ] [numerics](https://en.cppreference.com/w/c/numeric)
- [ ] [wide-character strings](https://en.cppreference.com/w/c/string/wide)
- [ ] [multibyte strings](https://en.cppreference.com/w/c/string/multibyte)
- [ ] [wide-character IO](https://en.cppreference.com/w/c/io)
- [ ] [localization](https://en.cppreference.com/w/c/locale)
- [ ] anything newer than C 11

[Program support](https://en.cppreference.com/w/c/program) utilities aren't intended.

