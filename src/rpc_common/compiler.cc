#include "compiler.hh"
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <execinfo.h>

#if 1
void print_stacktrace() {
    void *array[50];
    size_t size = backtrace(array, 50);
    backtrace_symbols_fd(array, size, 2);
}
#else
void print_stacktrace() {
    char pid[20];
    sprintf(pid, "%d", getpid());
    char program[1024];
    ssize_t r = readlink("/proc/self/exe", program, sizeof(program));
    if (r > 0) {
        program[r] = 0;
        int child_pid = fork();
        if (!child_pid) {
            dup2(2, 1);
            execlp("gdb", "gdb", program, pid, "--batch", "-n",
                   "-ex", "bt", NULL);
            abort();
        } else
            waitpid(child_pid, NULL, 0);
    }
}
#endif

void fail_mandatory_assert(const char* file, int line, const char* assertion, const char* message) {
    if (message)
	fprintf(stderr, "assertion \"%s\" [%s] failed: file \"%s\", line %d\n",
		message, assertion, file, line);
    else
	fprintf(stderr, "assertion \"%s\" failed: file \"%s\", line %d\n",
		assertion, file, line);
    print_stacktrace();
    abort();
}

void check_unaligned_access() {
    static_assert(HAVE_UNALIGNED_ACCESS, "uses unaligned access");
}
