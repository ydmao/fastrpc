#pragma once

void print_stacktrace();

void check_unaligned_access();

/** @brief assert macro that always checks its argument, even if NDEBUG */
extern void fail_mandatory_assert(const char *file, int line, const char *assertion,
				  const char *message = 0) __attribute__((noreturn));
#define mandatory_assert(x, ...) do { if (!(x)) fail_mandatory_assert(__FILE__, __LINE__, #x, ## __VA_ARGS__); } while (0)
