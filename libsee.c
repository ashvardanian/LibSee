/**
 *  @file   libsee.c
 *  @brief  LibSee is a single-file micro-benchmarking library,
 *          that tells exactly how much time your program spends on any call,
 *          and fuzzes their behavior to highlight correctness issues.
 *          It uses `LD_PRELOAD` to override standard library functions.
 *
 *  @author Ash Vardanian
 *  @date   March 4, 2024
 */
#define LIBSEE_VERSION_MAJOR 0
#define LIBSEE_VERSION_MINOR 0
#define LIBSEE_VERSION_PATCH 1

#if !defined(LIBSEE_MAX_THREADS) || LIBSEE_MAX_THREADS <= 0
#define LIBSEE_MAX_THREADS 1024
#endif

/*
 *  When enabled, the behaviour of the library is modified for fuzzy testing.
 *  Some function calls will start failing sporadically, highlighting potential issues
 *  in the calling application.
 *
 *  Examples include:
 *  > Weird random generator patterns.
 *  > Uneven monotnically increasing time.
 *  > In numerical functions add epsilon-sized error.
 *  > In `malloc`/`free` return NULL or free the same pointer twice.
 *  > In `memcpy` abort if the ranges overlap.
 *  > In `write`/`read` process smaller chunk and return value smaller than the length.
 *  > In `open`/`close` occasionally return -1.
 *  > In `fork`/`exec` occasionally return -1.
 */
#if !defined(LIBSEE_FUZZ)
#define LIBSEE_FUZZ 0
#endif

/*
 *  When enabled, the library will print the name of each function it is about to call,
 *  and then again, once the function has returned. Only used for educational purposes.
 */
#if !defined(LIBSEE_LOG_EVERYTHING)
#define LIBSEE_LOG_EVERYTHING 0
#endif

/*
 *  The issue here is that RTLD_NEXT is not defined by the posix standard.
 *  So the GNU people don't enable it unless you #define _GNU_SOURCE or -D_GNU_SOURCE.
 *  Other relevant pieces of POSIX are dlfcn.h and dlsym.h.
 *  Interestingly, the later mentions RTLD_NEXT. Apparently, the GNU people are a bit
 *  confused about what is an extension and what is not.
 *  Source: https://stackoverflow.com/a/1777507
 */
#define _GNU_SOURCE 1

/*
 *  All the `_s` functions are part of the C11 standard, but they are not part of the C99 standard.
 *  Moreover, you need to define `__STDC_WANT_LIB_EXT1__` to `1` to get the declarations of these functions.
 */
#define __STDC_WANT_LIB_EXT1__ 1

#include <dlfcn.h> // `RTLD_NEXT`

#include <errno.h>  // `errno_t`
#include <stddef.h> // `rsize_t`

#if !defined(__STDC_LIB_EXT1__)
typedef int errno_t;
typedef size_t rsize_t;
#endif

/**
 *  @brief  Contains the number of times each function was called.
 *
 *  One such structure is created for each thread.
 *  Every element is a `size_t` intialized to zero, so the entire structure can be zeroed with `memset`,
 *  or casted to an unsigned integers array and used for element-wise operations.
 */
typedef union thread_local_counters {
    struct {
        size_t strcpy;
        size_t strcpy_s;
        size_t strncpy;
        size_t strncpy_s;
        size_t strcat;
        size_t strcat_s;
        size_t strncat;
        size_t strncat_s;
        size_t strxfrm;
        size_t strlen;
        size_t strnlen_s;
        size_t strcmp;
        size_t strncmp;
        size_t strcoll;
        size_t strchr;
        size_t strrchr;
        size_t strspn;
        size_t strcspn;
        size_t strpbrk;
        size_t strstr;
        size_t strtok;
        size_t strtok_s;
        size_t memchr;
        size_t memcmp;
        size_t memset;
        size_t memset_s;
        size_t memcpy;
        size_t memcpy_s;
        size_t memmove;
        size_t memmove_s;
        size_t strerror;
        size_t strerror_s;
        size_t memmem;
        size_t memrchr;

        size_t malloc;
        size_t calloc;
        size_t realloc;
        size_t free;
        size_t aligned_alloc;

        size_t qsort;
        size_t qsort_s;
        size_t bsearch;
        size_t bsearch_s;

        size_t srand;
        size_t rand;

        size_t fopen;
        size_t freopen;
        size_t fclose;
        size_t fflush;
        size_t setbuf;
        size_t setvbuf;
        size_t fread;
        size_t fwrite;
        size_t fseek;
        size_t ftell;
        size_t fsetpos;
        size_t fgetpos;
        size_t rewind;
        size_t clearerr;
        size_t feof;
        size_t ferror;
        size_t perror;
        size_t scanf;
        size_t fscanf;
        size_t sscanf;
        size_t vscanf;
        size_t vfscanf;
        size_t vsscanf;
        size_t printf;
        size_t fprintf;
        size_t sprintf;
        size_t snprintf;
        size_t vprintf;
        size_t vfprintf;
        size_t vsprintf;
        size_t vsnprintf;

        size_t difftime;
        size_t time;
        size_t clock;
        size_t timespec_get;
        size_t timespec_getres;
        size_t asctime;
        size_t asctime_s;
        size_t ctime;
        size_t ctime_s;
        size_t strftime;
        size_t wcsftime;
        size_t gmtime;
        size_t gmtime_r;
        size_t gmtime_s;
        size_t localtime;
        size_t localtime_r;
        size_t localtime_s;
        size_t mktime;
    } named;

    size_t indexed[94];
} thread_local_counters;

#pragma region Function Pointers

typedef char *(*api_strcpy_t)(char *dest, char const *src);
typedef errno_t (*api_strcpy_s_t)(char *dest, rsize_t destsz, char const *src);
typedef char *(*api_strncpy_t)(char *dest, char const *src, size_t count);
typedef errno_t (*api_strncpy_s_t)(char *dest, rsize_t destsz, char const *src, rsize_t count);
typedef char *(*api_strcat_t)(char *dest, char const *src);
typedef errno_t (*api_strcat_s_t)(char *dest, rsize_t destsz, char const *src);
typedef char *(*api_strncat_t)(char *dest, char const *src, size_t count);
typedef errno_t (*api_strncat_s_t)(char *dest, rsize_t destsz, char const *src, rsize_t count);
typedef size_t (*api_strxfrm_t)(char *dest, char const *src, size_t count);
typedef size_t (*api_strlen_t)(char const *str);
typedef errno_t (*api_strnlen_s_t)(char const *str, rsize_t strsz, size_t *length);
typedef int (*api_strcmp_t)(char const *lhs, char const *rhs);
typedef int (*api_strncmp_t)(char const *lhs, char const *rhs, size_t count);
typedef int (*api_strcoll_t)(char const *lhs, char const *rhs);
typedef char *(*api_strchr_t)(char const *str, int ch);
typedef char *(*api_strrchr_t)(char const *str, int ch);
typedef size_t (*api_strspn_t)(char const *dest, char const *src);
typedef size_t (*api_strcspn_t)(char const *dest, char const *src);
typedef char *(*api_strpbrk_t)(char const *dest, char const *src);
typedef char *(*api_strstr_t)(char const *haystack, char const *needle);
typedef char *(*api_strtok_t)(char *str, char const *delim);
typedef errno_t (*api_strtok_s_t)(char *s, rsize_t *s_max, char const *delim, char **ptr);
typedef void *(*api_memchr_t)(void const *str, int ch, size_t max);
typedef int (*api_memcmp_t)(void const *lhs, void const *rhs, size_t count);
typedef void *(*api_memset_t)(void *dest, int ch, size_t count);
typedef errno_t (*api_memset_s_t)(void *dest, rsize_t destsz, int ch, rsize_t count);
typedef void *(*api_memcpy_t)(void *dest, void const *src, size_t count);
typedef errno_t (*api_memcpy_s_t)(void *dest, rsize_t destsz, void const *src, rsize_t count);
typedef void *(*api_memmove_t)(void *dest, void const *src, size_t count);
typedef errno_t (*api_memmove_s_t)(void *dest, rsize_t destsz, void const *src, rsize_t count);
typedef char *(*api_strerror_t)(int errnum);
typedef errno_t (*api_strerror_s_t)(char *buf, rsize_t bufsz, errno_t errnum);
typedef void *(*api_memmem_t)(void const *haystack, size_t haystacklen, void const *needle, size_t needlelen);
typedef void *(*api_memrchr_t)(void const *s, int c, size_t n);

typedef void *(*api_malloc_t)(size_t);
typedef void *(*api_calloc_t)(size_t, size_t);
typedef void *(*api_realloc_t)(void *, size_t);
typedef void (*api_free_t)(void *);
typedef void *(*api_aligned_alloc_t)(size_t, size_t);

typedef void (*api_qsort_t)(void *base, size_t count, size_t size, int (*compare)(void const *, void const *));
typedef void (*api_qsort_s_t)(void *base, rsize_t count, rsize_t size,
    int (*compare)(void const *, void const *, void *), void *context);
typedef void *(*api_bsearch_t)(void const *key, void const *base, size_t count, size_t size,
    int (*compare)(void const *, void const *));
typedef void *(*api_bsearch_s_t)(void const *key, void const *base, rsize_t count, rsize_t size,
    int (*compare)(void const *, void const *, void *), void *context);

typedef void (*api_srand_t)(unsigned seed);
typedef int (*api_rand_t)(void);

#pragma region Input / Output // Contents of `stdio.h`

#include <stdio.h> // `FILE`

typedef FILE *(*api_fopen_t)(char const *filename, char const *mode);
typedef FILE *(*api_freopen_t)(char const *filename, char const *mode, FILE *stream);
typedef int (*api_fclose_t)(FILE *stream);
typedef int (*api_fflush_t)(FILE *stream);
typedef void (*api_setbuf_t)(FILE *stream, char *buf);
typedef int (*api_setvbuf_t)(FILE *stream, char *buf, int mode, size_t size);
typedef size_t (*api_fread_t)(void *ptr, size_t size, size_t nmemb, FILE *stream);
typedef size_t (*api_fwrite_t)(void const *ptr, size_t size, size_t nmemb, FILE *stream);
typedef int (*api_fseek_t)(FILE *stream, long offset, int whence);
typedef long (*api_ftell_t)(FILE *stream);
typedef int (*api_fsetpos_t)(FILE *stream, fpos_t const *pos);
typedef int (*api_fgetpos_t)(FILE *stream, fpos_t *pos);
typedef void (*api_rewind_t)(FILE *stream);
typedef void (*api_clearerr_t)(FILE *stream);
typedef int (*api_feof_t)(FILE *stream);
typedef int (*api_ferror_t)(FILE *stream);
typedef void (*api_perror_t)(char const *s);

// Narrow character input
typedef int (*api_scanf_t)(char const *format, ...);
typedef int (*api_fscanf_t)(FILE *stream, char const *format, ...);
typedef int (*api_sscanf_t)(char const *str, char const *format, ...);
typedef int (*api_vscanf_t)(char const *format, va_list arg);
typedef int (*api_vfscanf_t)(FILE *stream, char const *format, va_list arg);
typedef int (*api_vsscanf_t)(char const *str, char const *format, va_list arg);

// Narrow character output
typedef int (*api_printf_t)(char const *format, ...);
typedef int (*api_fprintf_t)(FILE *stream, char const *format, ...);
typedef int (*api_sprintf_t)(char *str, char const *format, ...);
typedef int (*api_snprintf_t)(char *str, size_t size, char const *format, ...);
typedef int (*api_vprintf_t)(char const *format, va_list arg);
typedef int (*api_vfprintf_t)(FILE *stream, char const *format, va_list arg);
typedef int (*api_vsprintf_t)(char *str, char const *format, va_list arg);
typedef int (*api_vsnprintf_t)(char *str, size_t size, char const *format, va_list arg);

#pragma endregion

#pragma region Date and Time // Contents of `time.h`

#include <time.h> // `time_t`

typedef double (*api_difftime_t)(time_t end, time_t beginning);
typedef time_t (*api_time_t)(time_t *arg);
typedef clock_t (*api_clock_t)(void);
typedef int (*api_timespec_get_t)(struct timespec *ts, int base);
typedef int (*api_timespec_getres_t)(struct timespec *res, int base);
typedef char *(*api_asctime_t)(struct tm const *time_ptr);
typedef errno_t (*api_asctime_s_t)(char *buf, rsize_t bufsz, struct tm const *time_ptr);
typedef char *(*api_ctime_t)(time_t const *clock);
typedef errno_t (*api_ctime_s_t)(char *buf, rsize_t bufsz, time_t const *clock);
typedef size_t (*api_strftime_t)(char *s, size_t maxsize, char const *format, struct tm const *timeptr);
typedef size_t (*api_wcsftime_t)(wchar_t *wcs, size_t maxsize, wchar_t const *format, struct tm const *timeptr);
typedef struct tm *(*api_gmtime_t)(time_t const *timer);
typedef struct tm *(*api_gmtime_r_t)(time_t const *timer, struct tm *result);
typedef errno_t (*api_gmtime_s_t)(time_t const *timer, struct tm *result);
typedef struct tm *(*api_localtime_t)(time_t const *timer);
typedef struct tm *(*api_localtime_r_t)(time_t const *timer, struct tm *result);
typedef errno_t (*api_localtime_s_t)(time_t const *timer, struct tm *result);
typedef time_t (*api_mktime_t)(struct tm *timeptr);

#pragma endregion

#pragma endregion

/**
 *  @brief  Lookup table for LibC functionality from the underlying implementation.
 *          Just one such structure is reused between threads.
 */
typedef struct real_apis {
    api_strcpy_t strcpy;
    api_strcpy_s_t strcpy_s;
    api_strncpy_t strncpy;
    api_strncpy_s_t strncpy_s;
    api_strcat_t strcat;
    api_strcat_s_t strcat_s;
    api_strncat_t strncat;
    api_strncat_s_t strncat_s;
    api_strxfrm_t strxfrm;
    api_strlen_t strlen;
    api_strnlen_s_t strnlen_s;
    api_strcmp_t strcmp;
    api_strncmp_t strncmp;
    api_strcoll_t strcoll;
    api_strchr_t strchr;
    api_strrchr_t strrchr;
    api_strspn_t strspn;
    api_strcspn_t strcspn;
    api_strpbrk_t strpbrk;
    api_strstr_t strstr;
    api_strtok_t strtok;
    api_strtok_s_t strtok_s;
    api_memchr_t memchr;
    api_memcmp_t memcmp;
    api_memset_t memset;
    api_memset_s_t memset_s;
    api_memcpy_t memcpy;
    api_memcpy_s_t memcpy_s;
    api_memmove_t memmove;
    api_memmove_s_t memmove_s;
    api_strerror_t strerror;
    api_strerror_s_t strerror_s;
    api_memmem_t memmem;
    api_memrchr_t memrchr;

    api_malloc_t malloc;
    api_calloc_t calloc;
    api_realloc_t realloc;
    api_free_t free;
    api_aligned_alloc_t aligned_alloc;

    api_qsort_t qsort;
    api_qsort_s_t qsort_s;
    api_bsearch_t bsearch;
    api_bsearch_s_t bsearch_s;

    api_srand_t srand;
    api_rand_t rand;

    api_fopen_t fopen;
    api_freopen_t freopen;
    api_fclose_t fclose;
    api_fflush_t fflush;
    api_setbuf_t setbuf;
    api_setvbuf_t setvbuf;
    api_fread_t fread;
    api_fwrite_t fwrite;
    api_fseek_t fseek;
    api_ftell_t ftell;
    api_fsetpos_t fsetpos;
    api_fgetpos_t fgetpos;
    api_rewind_t rewind;
    api_clearerr_t clearerr;
    api_feof_t feof;
    api_ferror_t ferror;
    api_perror_t perror;
    api_scanf_t scanf;
    api_fscanf_t fscanf;
    api_sscanf_t sscanf;
    api_vscanf_t vscanf;
    api_vfscanf_t vfscanf;
    api_vsscanf_t vsscanf;
    api_printf_t printf;
    api_fprintf_t fprintf;
    api_sprintf_t sprintf;
    api_snprintf_t snprintf;
    api_vprintf_t vprintf;
    api_vfprintf_t vfprintf;
    api_vsprintf_t vsprintf;
    api_vsnprintf_t vsnprintf;

    api_difftime_t difftime;
    api_time_t time;
    api_clock_t clock;
    api_timespec_get_t timespec_get;
    api_timespec_getres_t timespec_getres;
    api_asctime_t asctime;
    api_asctime_s_t asctime_s;
    api_ctime_t ctime;
    api_ctime_s_t ctime_s;
    api_strftime_t strftime;
    api_wcsftime_t wcsftime;
    api_gmtime_t gmtime;
    api_gmtime_r_t gmtime_r;
    api_gmtime_s_t gmtime_s;
    api_localtime_t localtime;
    api_localtime_r_t localtime_r;
    api_localtime_s_t localtime_s;
    api_mktime_t mktime;
} real_apis;

#define COMPILE_TIME_ASSERT(predicate, message) typedef char message[(predicate) ? 1 : -1]
COMPILE_TIME_ASSERT(sizeof(real_apis) == sizeof(thread_local_counters), sizes_of_structs_must_be_equal);

static real_apis libsee_apis = {NULL};
static thread_local_counters libsee_thread_cycles[LIBSEE_MAX_THREADS] = {0};
static thread_local_counters libsee_thread_calls[LIBSEE_MAX_THREADS] = {0};

#pragma region Global Helpers

void libsee_initialize_if_not(void);

size_t libsee_get_cpu_index(void) {
    size_t cpu_index;
#ifdef __aarch64__
    // On 64-bit Arm (Aarch64) we can use the MPIDR_EL1 register to get the CPU index.
    // https://developer.arm.com/documentation/ddi0601/2020-12/AArch64-Registers/MPIDR-EL1--Multiprocessor-Affinity-Register
    size_t mpidr;
    asm volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
    cpu_index = mpidr & 0xFF; // Extract Affinity level 0 as CPU index
#elif defined(__x86_64__) || defined(__i386__)
    // On x86 we can use RDTSCP to get the CPU index.
    // https://en.wikipedia.org/wiki/Time_Stamp_Counter
    size_t cycle_count;
    asm volatile("rdtscp" : "=A"(cycle_count), "=c"(cpu_index));
#else
    cpu_index = 0;
#endif
    return cpu_index;
}

size_t libsee_get_cpu_cycle(void) {
    size_t cycle_count;
#ifdef __aarch64__
    // ARMv8 implementation
    asm volatile("mrs %0, cntvct_el0" : "=r"(cycle_count));
#elif defined(__x86_64__) || defined(__i386__)
    // x86 implementation, but note __rdtsc does not directly give size_t
    unsigned int lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    cycle_count = ((size_t)hi << 32) | lo;
#else
    // Placeholder for other architectures
    cycle_count = 0;
#endif
    return cycle_count;
}

void syscall_print(char const *buf, size_t count) {
    long ret;
#ifdef __aarch64__
    // Inline assembly syntax for making a system call in AArch64 Linux.
    // Uses the svc #0 instruction (Supervisor Call) to make a system call.
    // The system call number is passed in x8, and the arguments are in x0, x1, and x2.
    long syscall_write = (long)64; // System call number for write in AArch64 Linux
    long file_descriptor = (long)1;
    asm volatile("mov x0, %1\n" // First argument: file descriptor
                 "mov x1, %2\n" // Second argument: buffer address
                 "mov x2, %3\n" // Third argument: buffer size
                 "mov x8, %4\n" // System call number: SYS_write (64)
                 "svc #0\n"     // Make the system call
                 "mov %0, x0"   // Store the return value
                 : "=r"(ret)
                 : "r"(file_descriptor), "r"(buf), "r"((long)count), "r"(syscall_write)
                 : "x0", "x1", "x2", "x8", "memory");
#elif defined(__x86_64__) || defined(__i386__)
    // Inline assembly syntax for making a system call in x86-64 Linux.
    // Uses the syscall instruction, passing the system call number in rax,
    // and the call arguments in rdi, rsi, and rdx, respectively.
    long syscall_write = (long)1; // System call number for write in x86-64 Linux
    unsigned int file_descriptor = (unsigned int)1;
    asm volatile("syscall"
                 : "=a"(ret)
                 : "a"(syscall_write), "D"(file_descriptor), "S"(buf), "d"(count)
                 : "rcx", "r11", "memory");
    (void)ret;
#endif
    (void)buf;
    (void)count;
    (void)ret;
}

void reopen_stdout(void) {
    long ret;
    // Typically, opening a file would require a path, which complicates direct syscalls,
    // because syscalls don't handle C strings directly. However, here's a conceptual approach.
    char const path_to_dev_tty[9] = "/dev/tty";

#ifdef __aarch64__
    // For AArch64, the 'open' syscall is number 56.
    asm volatile("mov x0, %1\n"  // Pointer to the file path ("/dev/tty")
                 "mov w1, %w2\n" // Flags (O_WRONLY=1), use 'w1' to specify a 32-bit register explicitly if needed
                 "mov w2, %w3\n" // Mode (irrelevant for stdout, typically 0), again 'w2' for 32-bit
                 "mov x8, 56\n"  // Syscall number for 'open' in AArch64
                 "svc #0\n"      // Make the system call
                 "mov %0, x0"    // Output: File descriptor (or error)
                 : "=r"(ret)     // Output: File descriptor (or error)
                 : "r"(path_to_dev_tty), "r"(1), "r"(0) // Inputs
                 : "x0", "x1", "x2", "x8", "memory");   // Clobbered registers
#elif defined(__x86_64__)
    // For x86-64, the open syscall is number 2.
    asm volatile("syscall"
                 : "=a"(ret)                                    // Output: File descriptor (or error)
                 : "a"(2), "D"(path_to_dev_tty), "S"(1), "d"(0) // Inputs: syscall number, path, flags, mode
                 : "rcx", "r11", "memory");                     // Clobbered registers
#endif
    // If ret is a valid file descriptor, FD 1 can be duplicated to it, but this assumes FD 1 was valid and openable.
    (void)ret;
}

void close_stdout(void) {
    long ret;
#ifdef __aarch64__
    asm volatile("mov x0, 1\n"  // File descriptor for stdout
                 "mov x8, 57\n" // Syscall number for 'close' in AArch64
                 "svc #0\n"
                 "mov %0, x0"
                 : "=r"(ret)
                 : // No inputs besides the syscall number and FD
                 : "x0", "x8", "memory");
#elif defined(__x86_64__)
    asm volatile("syscall"
                 : "=a"(ret)
                 : "a"(3), "D"(1) // Inputs: syscall number for 'close', FD for stdout
                 : "rcx", "r11", "memory");
#endif
    (void)ret;
}

#if LIBSEE_LOG_EVERYTHING
#define libsee_log(str, count) syscall_print(str, count)
#else
#define libsee_log(str, count) (void)0
#endif

#define libsee_noreturn(function_name, ...)                                  \
    do {                                                                     \
        libsee_initialize_if_not();                                          \
        libsee_log(#function_name "-started\n", sizeof(#function_name) + 9); \
        size_t _cpu_index, _cycle_count_start, _cycle_count_end;             \
        _cpu_index = libsee_get_cpu_index();                                 \
        _cycle_count_start = libsee_get_cpu_cycle();                         \
        libsee_apis.function_name(__VA_ARGS__);                              \
        _cycle_count_end = libsee_get_cpu_cycle();                           \
        size_t cycle_count = _cycle_count_end - _cycle_count_start;          \
        libsee_thread_cycles[_cpu_index].named.function_name += cycle_count; \
        libsee_thread_calls[_cpu_index].named.function_name++;               \
        libsee_log(#function_name "-closed\n", sizeof(#function_name) + 8);  \
    } while (0)

#define libsee_return(function_name, return_type, ...)                       \
    do {                                                                     \
        libsee_initialize_if_not();                                          \
        libsee_log(#function_name "-started\n", sizeof(#function_name) + 9); \
        size_t _cpu_index, _cycle_count_start, _cycle_count_end;             \
        _cpu_index = libsee_get_cpu_index();                                 \
        _cycle_count_start = libsee_get_cpu_cycle();                         \
        return_type _result = libsee_apis.function_name(__VA_ARGS__);        \
        _cycle_count_end = libsee_get_cpu_cycle();                           \
        size_t cycle_count = _cycle_count_end - _cycle_count_start;          \
        libsee_thread_cycles[_cpu_index].named.function_name += cycle_count; \
        libsee_thread_calls[_cpu_index].named.function_name++;               \
        libsee_log(#function_name "-closed\n", sizeof(#function_name) + 8);  \
        return _result;                                                      \
    } while (0)

#define libsee_assign(returned_value, function_name, ...)                    \
    do {                                                                     \
        libsee_initialize_if_not();                                          \
        libsee_log(#function_name "-started\n", sizeof(#function_name) + 9); \
        size_t _cpu_index, _cycle_count_start, _cycle_count_end;             \
        _cpu_index = libsee_get_cpu_index();                                 \
        _cycle_count_start = libsee_get_cpu_cycle();                         \
        returned_value = libsee_apis.function_name(__VA_ARGS__);             \
        _cycle_count_end = libsee_get_cpu_cycle();                           \
        size_t cycle_count = _cycle_count_end - _cycle_count_start;          \
        libsee_thread_cycles[_cpu_index].named.function_name += cycle_count; \
        libsee_thread_calls[_cpu_index].named.function_name++;               \
        libsee_log(#function_name "-closed\n", sizeof(#function_name) + 8);  \
    } while (0)

#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(__GNUC__)
#define libsee_export __attribute__((dllexport))
#else
#define libsee_export __declspec(dllexport)
#endif
#else
#if __GNUC__ >= 4
#define libsee_export __attribute__((visibility("default")))
#else
#define libsee_export
#endif
#endif

#pragma endregion

#pragma region Strings // Contents of `string.h`

/** copies one string to another
 *  https://en.cppreference.com/w/c/string/byte/strcpy
 */
libsee_export char *strcpy(char *dest, char const *src) { libsee_return(strcpy, char *, dest, src); }
libsee_export errno_t strcpy_s(char *dest, rsize_t destsz, char const *src) {
    libsee_return(strcpy_s, errno_t, dest, destsz, src);
}

/** copies a certain amount of characters from one string to another
 *  https://en.cppreference.com/w/c/string/byte/strncpy
 */
libsee_export char *strncpy(char *dest, char const *src, size_t count) {
    libsee_return(strncpy, char *, dest, src, count);
}
libsee_export errno_t strncpy_s(char *dest, rsize_t destsz, char const *src, rsize_t count) {
    libsee_return(strncpy_s, errno_t, dest, destsz, src, count);
}

/** concatenates two strings
 *  https://en.cppreference.com/w/c/string/byte/strcat
 */
libsee_export char *strcat(char *dest, char const *src) { libsee_return(strcat, char *, dest, src); }
libsee_export errno_t strcat_s(char *dest, rsize_t destsz, char const *src) {
    libsee_return(strcat_s, errno_t, dest, destsz, src);
}

/** concatenates a certain amount of characters of two strings
 *  https://en.cppreference.com/w/c/string/byte/strncat
 */
libsee_export char *strncat(char *dest, char const *src, size_t count) {
    libsee_return(strncat, char *, dest, src, count);
}
libsee_export errno_t strncat_s(char *dest, rsize_t destsz, char const *src, rsize_t count) {
    libsee_return(strncat_s, errno_t, dest, destsz, src, count);
}

/** transform a string so that strcmp would produce the same result as strcoll
 *  https://en.cppreference.com/w/c/string/byte/strxfrm
 */
libsee_export size_t strxfrm(char *dest, char const *src, size_t count) {
    libsee_return(strxfrm, size_t, dest, src, count);
}

/** returns the length of a given string
 *  https://en.cppreference.com/w/c/string/byte/strlen
 */
libsee_export size_t strlen(char const *str) { libsee_return(strlen, size_t, str); }
libsee_export errno_t strnlen_s(char const *str, rsize_t strsz, size_t *length) {
    libsee_return(strnlen_s, errno_t, str, strsz, length);
}

/** compares two strings
 *  https://en.cppreference.com/w/c/string/byte/strcmp
 */
libsee_export int strcmp(char const *lhs, char const *rhs) { libsee_return(strcmp, int, lhs, rhs); }

/** compares a certain amount of characters of two strings
 *  https://en.cppreference.com/w/c/string/byte/strncmp
 */
libsee_export int strncmp(char const *lhs, char const *rhs, size_t count) {
    libsee_return(strncmp, int, lhs, rhs, count);
}

/** compares two strings in accordance to the current locale
 *  https://en.cppreference.com/w/c/string/byte/strcoll
 */
libsee_export int strcoll(char const *lhs, char const *rhs) { libsee_return(strcoll, int, lhs, rhs); }

/** finds the first occurrence of a character
 *  https://en.cppreference.com/w/c/string/byte/strchr
 */
libsee_export char *strchr(char const *str, int ch) { libsee_return(strchr, char *, str, ch); }

/** finds the last occurrence of a character
 *  https://en.cppreference.com/w/c/string/byte/strrchr
 */
libsee_export char *strrchr(char const *str, int ch) { libsee_return(strrchr, char *, str, ch); }

/** returns the length of the maximum initial segment that consists
 *  of only the characters found in another byte string
 *  https://en.cppreference.com/w/c/string/byte/strspn
 */
libsee_export size_t strspn(char const *dest, char const *src) { libsee_return(strspn, size_t, dest, src); }

/** returns the length of the maximum initial segment that consists
 *  of only the characters not found in another byte string
 *  https://en.cppreference.com/w/c/string/byte/strcspn
 */
libsee_export size_t strcspn(char const *dest, char const *src) { libsee_return(strcspn, size_t, dest, src); }

/** finds the first location of any character in one string, in another string
 *  https://en.cppreference.com/w/c/string/byte/strpbrk
 */
libsee_export char *strpbrk(char const *dest, char const *src) { libsee_return(strpbrk, char *, dest, src); }

/** finds the first occurrence of a substring of characters
 *  https://en.cppreference.com/w/c/string/byte/strstr
 */
libsee_export char *strstr(char const *haystack, char const *needle) {
    libsee_return(strstr, char *, haystack, needle);
}

/** finds the next token in a byte string
 *  https://en.cppreference.com/w/c/string/byte/strtok
 */
libsee_export char *strtok(char *str, char const *delim) { libsee_return(strtok, char *, str, delim); }
libsee_export errno_t strtok_s(char *s, rsize_t *s_max, char const *delim, char **ptr) {
    libsee_return(strtok_s, errno_t, s, s_max, delim, ptr);
}

/** searches an array for the first occurrence of a character
 *  https://en.cppreference.com/w/c/string/byte/memchr
 */
libsee_export void *memchr(void const *str, int ch, size_t max) { libsee_return(memchr, void *, str, ch, max); }

/** compares two buffers
 *  https://en.cppreference.com/w/c/string/byte/memcmp
 */
libsee_export int memcmp(void const *lhs, void const *rhs, size_t count) {
    libsee_return(memcmp, int, lhs, rhs, count);
}

/** fills a buffer with a character
 *  https://en.cppreference.com/w/c/string/byte/memset
 */
libsee_export void *memset(void *dest, int ch, size_t count) { libsee_return(memset, void *, dest, ch, count); }
libsee_export errno_t memset_s(void *dest, rsize_t destsz, int ch, rsize_t count) {
    libsee_return(memset_s, errno_t, dest, destsz, ch, count);
}

/** copies one buffer to another
 *  https://en.cppreference.com/w/c/string/byte/memcpy
 */
libsee_export void *memcpy(void *dest, void const *src, size_t count) {
    libsee_return(memcpy, void *, dest, src, count);
}
libsee_export errno_t memcpy_s(void *dest, rsize_t destsz, void const *src, rsize_t count) {
    libsee_return(memcpy_s, errno_t, dest, destsz, src, count);
}

/** moves one buffer to another
 *  https://en.cppreference.com/w/c/string/byte/memmove
 */
libsee_export void *memmove(void *dest, void const *src, size_t count) {
    libsee_return(memmove, void *, dest, src, count);
}
libsee_export errno_t memmove_s(void *dest, rsize_t destsz, void const *src, rsize_t count) {
    libsee_return(memmove_s, errno_t, dest, destsz, src, count);
}

/** returns a text version of a given error code
 *  https://en.cppreference.com/w/c/string/byte/strerror
 */
libsee_export char *strerror(int errnum) { libsee_return(strerror, char *, errnum); }
libsee_export errno_t strerror_s(char *buf, rsize_t bufsz, errno_t errnum) {
    libsee_return(strerror_s, errno_t, buf, bufsz, errnum);
}

/** relevant @b extensions for substring and reverse character search
 *  https://man7.org/linux/man-pages/man3/memmem.3.html
 */
libsee_export void *memmem(void const *haystack, size_t haystacklen, void const *needle, size_t needlelen) {
    libsee_return(memmem, void *, haystack, haystacklen, needle, needlelen);
}
libsee_export void *memrchr(void const *s, int c, size_t n) { libsee_return(memrchr, void *, s, c, n); }

#pragma endregion

#pragma region Numerics // Contents of `stdlib.h`

libsee_export void srand(unsigned seed) { libsee_noreturn(srand, seed); }
libsee_export int rand(void) { libsee_return(rand, int); }

/** common math functions
 *  https://en.cppreference.com/w/c/numeric/math
 */

/** type-generic functions
 *  https://en.cppreference.com/w/c/numeric/tgmath
 */

#pragma endregion

#pragma region Input / Output // Contents of `stdio.h`

#include <stdarg.h> // `va_start`

/** opens a file
 *  https://en.cppreference.com/w/c/io/fopen
 */
libsee_export FILE *fopen(const char *filename, char const *mode) { libsee_return(fopen, FILE *, filename, mode); }

/** reopens a file stream with a different file or mode
 *  https://en.cppreference.com/w/c/io/freopen
 */
libsee_export FILE *freopen(char const *filename, char const *mode, FILE *stream) {
    libsee_return(freopen, FILE *, filename, mode, stream);
}

/** closes a file
 *  https://en.cppreference.com/w/c/io/fclose
 */
libsee_export int fclose(FILE *stream) { libsee_return(fclose, int, stream); }

/** synchronizes an output stream with the actual file
 *  https://en.cppreference.com/w/c/io/fflush
 */
libsee_export int fflush(FILE *stream) { libsee_return(fflush, int, stream); }

/** sets the buffer for a file stream
 *  https://en.cppreference.com/w/c/io/setbuf
 */
libsee_export void setbuf(FILE *stream, char *buf) { libsee_noreturn(setbuf, stream, buf); }

/** sets the buffer and its size for a file stream
 *  https://en.cppreference.com/w/c/io/setvbuf
 *
 *  The setvbuf function may be used to specify the buffering for stream.
 */
libsee_export int setvbuf(FILE *stream, char *buf, int mode, size_t size) {
    libsee_return(setvbuf, int, stream, buf, mode, size);
}

/** reads from a file
 *  https://en.cppreference.com/w/c/io/fread
 */
libsee_export size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    libsee_return(fread, size_t, ptr, size, nmemb, stream);
}

/** writes to a file
 *  https://en.cppreference.com/w/c/io/fwrite
 */
libsee_export size_t fwrite(void const *ptr, size_t size, size_t nmemb, FILE *stream) {
    libsee_return(fwrite, size_t, ptr, size, nmemb, stream);
}

/** moves the file position indicator to a specific location in a file
 *  https://en.cppreference.com/w/c/io/fseek
 */
libsee_export int fseek(FILE *stream, long offset, int whence) { libsee_return(fseek, int, stream, offset, whence); }

/** returns the current file position indicator
 *  https://en.cppreference.com/w/c/io/ftell
 */
libsee_export long ftell(FILE *stream) { libsee_return(ftell, long, stream); }

/** sets the file position of the given stream to the given position
 *  https://en.cppreference.com/w/c/io/fsetpos
 */
libsee_export int fsetpos(FILE *stream, fpos_t const *pos) { libsee_return(fsetpos, int, stream, pos); }

/** gets the file position indicator
 *  https://en.cppreference.com/w/c/io/fgetpos
 */
libsee_export int fgetpos(FILE *stream, fpos_t *pos) { libsee_return(fgetpos, int, stream, pos); }

/** moves the file position indicator to the beginning of a file
 *  https://en.cppreference.com/w/c/io/rewind
 */
libsee_export void rewind(FILE *stream) { libsee_noreturn(rewind, stream); }

/** clears errors
 *  https://en.cppreference.com/w/c/io/clearerr
 */
libsee_export void clearerr(FILE *stream) { libsee_noreturn(clearerr, stream); }

/** checks for the end-of-file
 *  https://en.cppreference.com/w/c/io/feof
 */
libsee_export int feof(FILE *stream) { libsee_return(feof, int, stream); }

/** checks for a file error
 *  https://en.cppreference.com/w/c/io/ferror
 */
libsee_export int ferror(FILE *stream) { libsee_return(ferror, int, stream); }

/** displays a character string corresponding of the current error to stderr
 *  https://en.cppreference.com/w/c/io/perror
 */
libsee_export void perror(char const *s) { libsee_noreturn(perror, s); }

/** reads formatted input from stdin
 *  https://en.cppreference.com/w/c/io/fscanf
 */
libsee_export int scanf(char const *format, ...) {
    va_list args;
    int result;
    va_start(args, format);
    libsee_assign(result, vscanf, format, args);
    va_end(args);
    return result;
}

libsee_export int fscanf(FILE *stream, char const *format, ...) {
    va_list args;
    int result;
    va_start(args, format);
    libsee_assign(result, vfscanf, stream, format, args);
    va_end(args);
    return result;
}

libsee_export int sscanf(char const *str, char const *format, ...) {
    va_list args;
    int result;
    va_start(args, format);
    libsee_assign(result, vsscanf, str, format, args);
    va_end(args);
    return result;
}

/** reads formatted input from stdin
 *  https://en.cppreference.com/w/c/io/vscanf
 */
libsee_export int vscanf(char const *format, va_list vlist) { libsee_return(vscanf, int, format, vlist); }

libsee_export int vfscanf(FILE *stream, char const *format, va_list vlist) {
    libsee_return(vfscanf, int, stream, format, vlist);
}

libsee_export int vsscanf(char const *str, char const *format, va_list vlist) {
    libsee_return(vsscanf, int, str, format, vlist);
}

/** prints formatted output to stdout
 *  https://en.cppreference.com/w/c/io/fprintf
 */
libsee_export int printf(char const *format, ...) {
    int result;
    va_list args;
    va_start(args, format);
    libsee_assign(result, vprintf, format, args);
    va_end(args);
    return result;
}

libsee_export int fprintf(FILE *stream, char const *format, ...) {
    int result;
    va_list args;
    va_start(args, format);
    libsee_assign(result, vfprintf, stream, format, args);
    va_end(args);
    return result;
}

#undef sprintf  // For MacOS
#undef vsprintf // For MacOS
libsee_export int sprintf(char *str, char const *format, ...) {
    int result;
    va_list args;
    va_start(args, format);
    libsee_assign(result, vsprintf, str, format, args);
    va_end(args);
    return result;
}

#undef snprintf  // For MacOS
#undef vsnprintf // For MacOS
libsee_export int snprintf(char *str, size_t size, char const *format, ...) {
    int result;
    va_list args;
    va_start(args, format);
    libsee_assign(result, vsnprintf, str, size, format, args);
    va_end(args);
    return result;
}

/** prints formatted output to stdout
 *  https://en.cppreference.com/w/c/io/fprintf
 */
libsee_export int vprintf(char const *format, va_list vlist) { libsee_return(vprintf, int, format, vlist); }

libsee_export int vfprintf(FILE *stream, char const *format, va_list vlist) {
    libsee_return(vfprintf, int, stream, format, vlist);
}

libsee_export int vsprintf(char *str, char const *format, va_list vlist) {
    libsee_return(vsprintf, int, str, format, vlist);
}

libsee_export int vsnprintf(char *str, size_t size, char const *format, va_list vlist) {
    libsee_return(vsnprintf, int, str, size, format, vlist);
}

#pragma endregion

#pragma region Date and Time // Contents of `time.h`

/** computes the difference between times
 *  https://en.cppreference.com/w/c/chrono/difftime
 */
libsee_export double difftime(time_t end, time_t beginning) { libsee_return(difftime, double, end, beginning); }

/** returns the current calendar time of the system as time since epoch
 *  https://en.cppreference.com/w/c/chrono/time
 */
libsee_export time_t time(time_t *arg) { libsee_return(time, time_t, arg); }

/** returns raw processor clock time since the program is started
 *  https://en.cppreference.com/w/c/chrono/clock
 */
libsee_export clock_t clock(void) { libsee_return(clock, clock_t); }

/** returns the calendar time in seconds and nanoseconds based on a given time base
 *  https://en.cppreference.com/w/c/chrono/timespec_get
 */
libsee_export int timespec_get(struct timespec *ts, int base) { libsee_return(timespec_get, int, ts, base); }

/** returns the resolution of calendar time based on a given time base
 *  https://en.cppreference.com/w/c/chrono/timespec_getres
 */
libsee_export int timespec_getres(struct timespec *res, int base) { libsee_return(timespec_getres, int, res, base); }

/** converts a tm object to a textual representation
 *  https://en.cppreference.com/w/c/chrono/asctime
 */
libsee_export char *asctime(struct tm const *time_ptr) { libsee_return(asctime, char *, time_ptr); }
libsee_export errno_t asctime_s(char *buf, rsize_t bufsz, struct tm const *time_ptr) {
    libsee_return(asctime_s, errno_t, buf, bufsz, time_ptr);
}

/** converts a time_t object to a textual representation
 *  https://en.cppreference.com/w/c/chrono/ctime
 */
libsee_export char *ctime(time_t const *clock) { libsee_return(ctime, char *, clock); }
libsee_export errno_t ctime_s(char *buf, rsize_t bufsz, time_t const *clock) {
    libsee_return(ctime_s, errno_t, buf, bufsz, clock);
}

/** converts a tm object to custom textual representation
 *  https://en.cppreference.com/w/c/chrono/strftime
 */
libsee_export size_t strftime(char *s, size_t maxsize, char const *format, struct tm const *timeptr) {
    libsee_return(strftime, size_t, s, maxsize, format, timeptr);
}

/** converts a tm object to custom wide string textual representation
 *  https://en.cppreference.com/w/c/chrono/wcsftime
 */
libsee_export size_t wcsftime(wchar_t *wcs, size_t maxsize, wchar_t const *format, struct tm const *timeptr) {
    libsee_return(wcsftime, size_t, wcs, maxsize, format, timeptr);
}

/** converts time since epoch to calendar time expressed as Coordinated Universal Time (UTC)
 *  https://en.cppreference.com/w/c/chrono/gmtime
 */
libsee_export struct tm *gmtime(time_t const *timer) { libsee_return(gmtime, struct tm *, timer); }
libsee_export struct tm *gmtime_r(time_t const *timer, struct tm *result) {
    libsee_return(gmtime_r, struct tm *, timer, result);
}
libsee_export errno_t gmtime_s(time_t const *timer, struct tm *result) {
    libsee_return(gmtime_s, errno_t, timer, result);
}

/** converts time since epoch to calendar time expressed as local time
 *  https://en.cppreference.com/w/c/chrono/localtime
 */
libsee_export struct tm *localtime(time_t const *timer) { libsee_return(localtime, struct tm *, timer); }
libsee_export struct tm *localtime_r(time_t const *timer, struct tm *result) {
    libsee_return(localtime_r, struct tm *, timer, result);
}
libsee_export errno_t localtime_s(time_t const *timer, struct tm *result) {
    libsee_return(localtime_s, errno_t, timer, result);
}

/** converts calendar time to time since epoch
 *  https://en.cppreference.com/w/c/chrono/mktime
 */
libsee_export time_t mktime(struct tm *timeptr) { libsee_return(mktime, time_t, timeptr); }

#pragma endregion

#pragma region Memory Management // Contents of `stdlib.h`

/** allocates memory
 *  https://en.cppreference.com/w/c/memory/malloc
 */
libsee_export void *malloc(size_t size) { libsee_return(malloc, void *, size); }
libsee_export void *calloc(size_t num, size_t size) { libsee_return(calloc, void *, num, size); }
libsee_export void *realloc(void *ptr, size_t size) { libsee_return(realloc, void *, ptr, size); }

/** deallocates memory
 *  https://en.cppreference.com/w/c/memory/free
 */
libsee_export void free(void *ptr) { libsee_noreturn(free, ptr); }

/** allocates aligned memory
 *  https://en.cppreference.com/w/c/memory/aligned_alloc
 */
libsee_export void *aligned_alloc(size_t alignment, size_t size) {
    libsee_return(aligned_alloc, void *, alignment, size);
}

#pragma endregion

#pragma region Algorithms // Contents of `stdlib.h`

/** sorts an array
 *  https://en.cppreference.com/w/c/algorithm/qsort
 *
 *  Despite the name, neither C nor POSIX standards require this function to be implemented
 *  using quicksort or make any complexity or stability guarantees.
 */
libsee_export void qsort(void *base, size_t count, size_t size, int (*compare)(void const *, void const *)) {
    libsee_noreturn(qsort, base, count, size, compare);
}
libsee_export void qsort_s(void *base, rsize_t count, rsize_t size, int (*compare)(void const *, void const *, void *),
    void *context) {
    libsee_noreturn(qsort_s, base, count, size, compare, context);
}

/** searches an array for a given value
 *  https://en.cppreference.com/w/c/algorithm/bsearch
 *
 *  Despite the name, neither C nor POSIX standards require this function to be implemented
 *  using binary search or make any complexity guarantees.
 */
libsee_export void *bsearch(void const *key, void const *base, size_t count, size_t size,
    int (*compare)(void const *, void const *)) {
    libsee_return(bsearch, void *, key, base, count, size, compare);
}
libsee_export void *bsearch_s(void const *key, void const *base, rsize_t count, rsize_t size,
    int (*compare)(void const *, void const *, void *), void *context) {
    libsee_return(bsearch_s, void *, key, base, count, size, compare, context);
}

#pragma endregion

#pragma region Shared Implementation Components

typedef struct libsee_name_stats {
    char const *function_name;
    size_t total_cycles;
    size_t total_calls;
} libsee_name_stats;

void libsee_initialize(void) {

    // Initialize all the cycles to zeros, without using `memset`
    size_t *cycles = libsee_thread_cycles[0].indexed;
    size_t *calls = libsee_thread_calls[0].indexed;
    size_t total_counters_per_thread = sizeof(thread_local_counters) / sizeof(size_t);
    size_t total_counters_across_threads = LIBSEE_MAX_THREADS * total_counters_per_thread;
    for (size_t i = 0; i < total_counters_across_threads; i++) cycles[i] = calls[i] = 0;

    // Load the symbols from the underlying implementation
    real_apis *apis = &libsee_apis;

    apis->strcpy = (api_strcpy_t)dlsym(RTLD_NEXT, "strcpy");
    apis->strncpy = (api_strncpy_t)dlsym(RTLD_NEXT, "strncpy");
    apis->strcat = (api_strcat_t)dlsym(RTLD_NEXT, "strcat");
    apis->strncat = (api_strncat_t)dlsym(RTLD_NEXT, "strncat");
    apis->strxfrm = (api_strxfrm_t)dlsym(RTLD_NEXT, "strxfrm");
    apis->strlen = (api_strlen_t)dlsym(RTLD_NEXT, "strlen");
    apis->strcmp = (api_strcmp_t)dlsym(RTLD_NEXT, "strcmp");
    apis->strncmp = (api_strncmp_t)dlsym(RTLD_NEXT, "strncmp");
    apis->strcoll = (api_strcoll_t)dlsym(RTLD_NEXT, "strcoll");
    apis->strchr = (api_strchr_t)dlsym(RTLD_NEXT, "strchr");
    apis->strrchr = (api_strrchr_t)dlsym(RTLD_NEXT, "strrchr");
    apis->strspn = (api_strspn_t)dlsym(RTLD_NEXT, "strspn");
    apis->strcspn = (api_strcspn_t)dlsym(RTLD_NEXT, "strcspn");
    apis->strpbrk = (api_strpbrk_t)dlsym(RTLD_NEXT, "strpbrk");
    apis->strstr = (api_strstr_t)dlsym(RTLD_NEXT, "strstr");
    apis->strtok = (api_strtok_t)dlsym(RTLD_NEXT, "strtok");
    apis->memchr = (api_memchr_t)dlsym(RTLD_NEXT, "memchr");
    apis->memcmp = (api_memcmp_t)dlsym(RTLD_NEXT, "memcmp");
    apis->memset = (api_memset_t)dlsym(RTLD_NEXT, "memset");
    apis->memcpy = (api_memcpy_t)dlsym(RTLD_NEXT, "memcpy");
    apis->memmove = (api_memmove_t)dlsym(RTLD_NEXT, "memmove");
    apis->strerror = (api_strerror_t)dlsym(RTLD_NEXT, "strerror");
    apis->memmem = (api_memmem_t)dlsym(RTLD_NEXT, "memmem");
    apis->memrchr = (api_memrchr_t)dlsym(RTLD_NEXT, "memrchr");

    apis->malloc = (api_malloc_t)dlsym(RTLD_NEXT, "malloc");
    apis->calloc = (api_calloc_t)dlsym(RTLD_NEXT, "calloc");
    apis->realloc = (api_realloc_t)dlsym(RTLD_NEXT, "realloc");
    apis->free = (api_free_t)dlsym(RTLD_NEXT, "free");
    apis->aligned_alloc = (api_aligned_alloc_t)dlsym(RTLD_NEXT, "aligned_alloc");

    apis->qsort = (api_qsort_t)dlsym(RTLD_NEXT, "qsort");
    apis->bsearch = (api_bsearch_t)dlsym(RTLD_NEXT, "bsearch");

    apis->srand = (api_srand_t)dlsym(RTLD_NEXT, "srand");
    apis->rand = (api_rand_t)dlsym(RTLD_NEXT, "rand");

    apis->fopen = (api_fopen_t)dlsym(RTLD_NEXT, "fopen");
    apis->freopen = (api_freopen_t)dlsym(RTLD_NEXT, "freopen");
    apis->fclose = (api_fclose_t)dlsym(RTLD_NEXT, "fclose");
    apis->fflush = (api_fflush_t)dlsym(RTLD_NEXT, "fflush");
    apis->setbuf = (api_setbuf_t)dlsym(RTLD_NEXT, "setbuf");
    apis->setvbuf = (api_setvbuf_t)dlsym(RTLD_NEXT, "setvbuf");
    apis->fread = (api_fread_t)dlsym(RTLD_NEXT, "fread");
    apis->fwrite = (api_fwrite_t)dlsym(RTLD_NEXT, "fwrite");
    apis->fseek = (api_fseek_t)dlsym(RTLD_NEXT, "fseek");
    apis->ftell = (api_ftell_t)dlsym(RTLD_NEXT, "ftell");
    apis->fsetpos = (api_fsetpos_t)dlsym(RTLD_NEXT, "fsetpos");
    apis->fgetpos = (api_fgetpos_t)dlsym(RTLD_NEXT, "fgetpos");
    apis->rewind = (api_rewind_t)dlsym(RTLD_NEXT, "rewind");
    apis->clearerr = (api_clearerr_t)dlsym(RTLD_NEXT, "clearerr");
    apis->feof = (api_feof_t)dlsym(RTLD_NEXT, "feof");
    apis->ferror = (api_ferror_t)dlsym(RTLD_NEXT, "ferror");
    apis->perror = (api_perror_t)dlsym(RTLD_NEXT, "perror");
    apis->scanf = (api_scanf_t)dlsym(RTLD_NEXT, "scanf");
    apis->fscanf = (api_fscanf_t)dlsym(RTLD_NEXT, "fscanf");
    apis->sscanf = (api_sscanf_t)dlsym(RTLD_NEXT, "sscanf");
    apis->vscanf = (api_vscanf_t)dlsym(RTLD_NEXT, "vscanf");
    apis->vfscanf = (api_vfscanf_t)dlsym(RTLD_NEXT, "vfscanf");
    apis->vsscanf = (api_vsscanf_t)dlsym(RTLD_NEXT, "vsscanf");
    apis->printf = (api_printf_t)dlsym(RTLD_NEXT, "printf");
    apis->fprintf = (api_fprintf_t)dlsym(RTLD_NEXT, "fprintf");
    apis->sprintf = (api_sprintf_t)dlsym(RTLD_NEXT, "sprintf");
    apis->snprintf = (api_snprintf_t)dlsym(RTLD_NEXT, "snprintf");
    apis->vprintf = (api_vprintf_t)dlsym(RTLD_NEXT, "vprintf");
    apis->vfprintf = (api_vfprintf_t)dlsym(RTLD_NEXT, "vfprintf");
    apis->vsprintf = (api_vsprintf_t)dlsym(RTLD_NEXT, "vsprintf");
    apis->vsnprintf = (api_vsnprintf_t)dlsym(RTLD_NEXT, "vsnprintf");

    apis->difftime = (api_difftime_t)dlsym(RTLD_NEXT, "difftime");
    apis->time = (api_time_t)dlsym(RTLD_NEXT, "time");
    apis->clock = (api_clock_t)dlsym(RTLD_NEXT, "clock");
    apis->timespec_get = (api_timespec_get_t)dlsym(RTLD_NEXT, "timespec_get");
    apis->timespec_getres = (api_timespec_getres_t)dlsym(RTLD_NEXT, "timespec_getres");
    apis->asctime = (api_asctime_t)dlsym(RTLD_NEXT, "asctime");
    apis->ctime = (api_ctime_t)dlsym(RTLD_NEXT, "ctime");
    apis->strftime = (api_strftime_t)dlsym(RTLD_NEXT, "strftime");
    apis->wcsftime = (api_wcsftime_t)dlsym(RTLD_NEXT, "wcsftime");
    apis->gmtime = (api_gmtime_t)dlsym(RTLD_NEXT, "gmtime");
    apis->gmtime_r = (api_gmtime_r_t)dlsym(RTLD_NEXT, "gmtime_r");
    apis->localtime = (api_localtime_t)dlsym(RTLD_NEXT, "localtime");
    apis->localtime_r = (api_localtime_r_t)dlsym(RTLD_NEXT, "localtime_r");
    apis->mktime = (api_mktime_t)dlsym(RTLD_NEXT, "mktime");

#if defined(__STDC_LIB_EXT1__)
    apis->strcpy_s = (api_strcpy_s_t)dlsym(RTLD_NEXT, "strcpy_s");
    apis->strncpy_s = (api_strncpy_s_t)dlsym(RTLD_NEXT, "strncpy_s");
    apis->strcat_s = (api_strcat_s_t)dlsym(RTLD_NEXT, "strcat_s");
    apis->strncat_s = (api_strncat_s_t)dlsym(RTLD_NEXT, "strncat_s");
    apis->strnlen_s = (api_strnlen_s_t)dlsym(RTLD_NEXT, "strnlen_s");
    apis->strtok_s = (api_strtok_s_t)dlsym(RTLD_NEXT, "strtok_s");
    apis->memset_s = (api_memset_s_t)dlsym(RTLD_NEXT, "memset_s");
    apis->memcpy_s = (api_memcpy_s_t)dlsym(RTLD_NEXT, "memcpy_s");
    apis->memmove_s = (api_memmove_s_t)dlsym(RTLD_NEXT, "memmove_s");
    apis->strerror_s = (api_strerror_s_t)dlsym(RTLD_NEXT, "strerror_s");
    apis->qsort_s = (api_qsort_s_t)dlsym(RTLD_NEXT, "qsort_s");
    apis->bsearch_s = (api_bsearch_s_t)dlsym(RTLD_NEXT, "bsearch_s");
    apis->asctime_s = (api_asctime_s_t)dlsym(RTLD_NEXT, "asctime_s");
    apis->ctime_s = (api_ctime_s_t)dlsym(RTLD_NEXT, "ctime_s");
    apis->gmtime_s = (api_gmtime_s_t)dlsym(RTLD_NEXT, "gmtime_s");
    apis->localtime_s = (api_localtime_s_t)dlsym(RTLD_NEXT, "localtime_s");
#endif
}

size_t libsee_print_size(size_t number, char thousands_separator, char *buffer) {
    if (number == 0) {
        buffer[0] = '0';
        buffer[1] = '\0'; // Null terminate the string
        return 1;
    }

    size_t temp = number;
    size_t digits = 0;
    while (temp > 0) {
        temp /= 10;
        digits++;
    }

    // Calculate the total length including commas
    size_t total_length = digits + (digits - 1) / 3;

    temp = number;
    size_t index = total_length; // Start filling the buffer from the end
    buffer[index--] = '\0';      // Null-terminate the string

    for (size_t i = 0; i < digits; i++) {
        // Insert comma every three digits
        if (i > 0 && i % 3 == 0) buffer[index--] = thousands_separator;
        buffer[index--] = (temp % 10) + '0'; // Convert digit to character
        temp /= 10;
    }

    return total_length;
}

size_t libsee_print_double(double number, char thousands_separator, size_t decimal_points, char *buffer) {
    // Handle negative numbers
    if (number < 0) {
        *buffer++ = '-';
        number = -number; // Make the number positive for formatting
    }

    // Split number into integer and fractional parts
    double integer_part = (size_t)number;
    double fractional_part = number - integer_part;

    // Convert integer part to string with thousands separator
    size_t integer_length = libsee_print_size((size_t)integer_part, thousands_separator, buffer);

    // Add decimal point
    buffer[integer_length] = '.';
    size_t total_length = integer_length + 1; // +1 for the decimal point

    // Handle fractional part
    for (size_t i = 0; i < decimal_points; i++) {
        fractional_part *= 10;                   // Shift fractional part to the left
        int digit = (int)(fractional_part) % 10; // Get next digit
        buffer[total_length++] = '0' + digit;    // Convert digit to character and store
    }

    // Null-terminate the string
    buffer[total_length] = '\0';

    return total_length; // Return length of the string
}

size_t libsee_pad_buffer(char *buffer, size_t current_length, size_t target_length) {
    while (current_length < target_length) { buffer[current_length++] = ' '; }
    buffer[current_length] = '\0'; // Null-terminate the padded string
    return target_length;
}

void libsee_finalize(void) {
    reopen_stdout();

#if LIBSEE_LOG_EVERYTHING
    syscall_print("Finalizing\n", 11);
#endif

    size_t cycles_across_threads = 0;
    size_t counters_per_thread = sizeof(thread_local_counters) / sizeof(size_t);
    for (size_t i = 0; i < counters_per_thread; i++)
        for (size_t t = 0; t < LIBSEE_MAX_THREADS; t++) cycles_across_threads += libsee_thread_cycles[t].indexed[i];

    // Aggregate stats from all threads into the first one.
    for (size_t t = 1; t < LIBSEE_MAX_THREADS; t++) {
        for (size_t j = 0; j < counters_per_thread; j++) {
            libsee_thread_cycles[0].indexed[j] += libsee_thread_cycles[t].indexed[j];
            libsee_thread_calls[0].indexed[j] += libsee_thread_calls[t].indexed[j];
        }
    }

    // Create an on-stack array of all of those counters, populate them, and sort by the most called functions.
    libsee_name_stats named_stats[] = {// Strings
        {"strcpy"}, {"strcpy_s"}, {"strncpy"}, {"strncpy_s"}, {"strcat"}, {"strcat_s"}, {"strncat"}, {"strncat_s"},
        {"strxfrm"}, {"strlen"}, {"strnlen_s"}, {"strcmp"}, {"strncmp"}, {"strcoll"}, {"strchr"}, {"strrchr"},
        {"strspn"}, {"strcspn"}, {"strpbrk"}, {"strstr"}, {"strtok"}, {"strtok_s"}, {"memchr"}, {"memcmp"}, {"memset"},
        {"memset_s"}, {"memcpy"}, {"memcpy_s"}, {"memmove"}, {"memmove_s"}, {"strerror"}, {"strerror_s"}, {"memmem"},
        {"memrchr"},
        // Heap
        {"malloc"}, {"calloc"}, {"realloc"}, {"free"}, {"aligned_alloc"},
        // Algorithms
        {"qsort"}, {"qsort_s"}, {"bsearch"}, {"bsearch_s"},
        // Numerics
        {"srand"}, {"rand"},
        // I/O
        {"fopen"}, {"freopen"}, {"fclose"}, {"fflush"}, {"setbuf"}, {"setvbuf"}, {"fread"}, {"fwrite"}, {"fseek"},
        {"ftell"}, {"fsetpos"}, {"fgetpos"}, {"rewind"}, {"clearerr"}, {"feof"}, {"ferror"}, {"perror"}, {"scanf"},
        {"fscanf"}, {"sscanf"}, {"vscanf"}, {"vfscanf"}, {"vsscanf"}, {"printf"}, {"fprintf"}, {"sprintf"},
        {"snprintf"}, {"vprintf"}, {"vfprintf"}, {"vsprintf"}, {"vsnprintf"},
        // Time
        {"difftime"}, {"time"}, {"clock"}, {"timespec_get"}, {"timespec_getres"}, {"asctime"}, {"asctime_s"}, {"ctime"},
        {"ctime_s"}, {"strftime"}, {"wcsftime"}, {"gmtime"}, {"gmtime_r"}, {"gmtime_s"}, {"localtime"}, {"localtime_r"},
        {"localtime_s"}, {"mktime"}};

    COMPILE_TIME_ASSERT(sizeof(real_apis) / sizeof(void *) == sizeof(named_stats) / sizeof(libsee_name_stats),
        number_of_counters_must_be_equal);

    // Assigning the total cycles for each function
    for (size_t i = 0; i < counters_per_thread; i++) {
        named_stats[i].total_calls = libsee_thread_calls[0].indexed[i];
        named_stats[i].total_cycles = libsee_thread_cycles[0].indexed[i];
    }

    // Sort the `named_stats` array with the simplest algorithm possible,
    // asymptotic complexity is not a concern here. BUBBLE SORT FOR THE WIN! I'm eight again :)
    for (size_t i = 0; i < counters_per_thread - 1; i++) {
        for (size_t j = 0; j < counters_per_thread - i - 1; j++) {
            if (named_stats[j].total_cycles < named_stats[j + 1].total_cycles) {
                libsee_name_stats temp = named_stats[j];
                named_stats[j] = named_stats[j + 1];
                named_stats[j + 1] = temp;
            }
        }
    }

    // Print them in a descending order of usage, manually formatting the output
    // as we can't rely on the presence of `printf` or `fprintf`.

#if LIBSEE_LOG_EVERYTHING
    syscall_print("LibSee function usage report (in descending order of CPU cycles):\n", 52);
#endif
    syscall_print("----------------------------------LIBSEE----------------------------------------\n", 81);
    size_t column_widths[] = {20, 40, 15, 15};

    // Print headers
    syscall_print("function,           cycles,                                 calls,         share\n", 81);

    // Print the sorted stats
    for (size_t i = 0; i < counters_per_thread; i++) {
        char stat_line[256];
        size_t stat_line_length = 0;
        char const *function_name = named_stats[i].function_name;
        size_t total_cycles = named_stats[i].total_cycles;
        size_t total_calls = named_stats[i].total_calls;
        double percent_cycles = (double)total_cycles * 100.0 / (double)cycles_across_threads;
        if (total_cycles == 0) { continue; } // Skip functions that were never called.

        // Append function name and pad
        size_t name_length = 0;
        while (function_name[name_length]) { stat_line[stat_line_length++] = function_name[name_length++]; }
        stat_line[stat_line_length++] = ',';
        stat_line_length = libsee_pad_buffer(stat_line, stat_line_length, column_widths[0]);

        // Convert and append total_cycles with padding
        stat_line_length += libsee_print_size(total_cycles, ' ', stat_line + stat_line_length);
        stat_line[stat_line_length++] = ',';
        stat_line_length = libsee_pad_buffer(stat_line, stat_line_length, column_widths[0] + column_widths[1]);

        // Convert and append total_calls with padding
        stat_line_length += libsee_print_size(total_calls, ' ', stat_line + stat_line_length);
        stat_line[stat_line_length++] = ',';
        stat_line_length =
            libsee_pad_buffer(stat_line, stat_line_length, column_widths[0] + column_widths[1] + column_widths[2]);

        // Convert and append percent_cycles with specified decimal points (e.g., 2) and padding
        stat_line_length += libsee_print_double(percent_cycles, ' ', 2, stat_line + stat_line_length);

        // We don't need padding at the end, just the newline.
        // stat_line_length = libsee_pad_buffer(stat_line, stat_line_length,
        //     column_widths[0] + column_widths[1] + column_widths[2] + column_widths[3]);
        stat_line[stat_line_length++] = '\n';

        // Ensure the line is null-terminated.
        stat_line[stat_line_length] = '\0';
        syscall_print(stat_line, stat_line_length);
    }

    syscall_print("----------------------------------LIBSEE----------------------------------------\n", 81);
    close_stdout();
}

// The world is messed up...
// In an ideal universe I'd use the constructor to call the `libsee_initialize` function,
// and in destructor to call `libsee_finalize`. But, we're not in an ideal universe.
//
//      __attribute__((constructor)) void libsee_initialize_gcc(void) { libsee_initialize(); }
//      __attribute__((destructor)) void libsee_finalize_gcc(void) { libsee_finalize(); }
//
// We should only call `libsee_initialize` once the other libraries and symbols are loaded.
// For that we use `libsee_initialize_if_not` and call it every time a LibC symbol is requested.
// Similarly, we should call `libsee_finalize` and print results before the exit sequence starts.
// Since glibc 2.2.3, atexit() (and on_exit()) can be used within a shared library to establish
// functions that are called when the shared library is unloaded... but the problem is that
// STDIN and STDOUT descriptors are closed before the destructor is called... So I use inline Asm
// to reopen them and close again.
void libsee_initialize_if_not(void) {
    static int initialized = 0;
    if (__builtin_expect(initialized == 0, 0)) {
        libsee_initialize();
        initialized = 1;
    }
}

__attribute__((constructor)) void libsee_initialize_gcc(void) {}
__attribute__((destructor)) void libsee_finalize_gcc(void) { libsee_finalize(); }
