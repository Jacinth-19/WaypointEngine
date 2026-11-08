// Waypoint Engine — assertion helpers.
// SPDX-License-Identifier: Apache-2.0
//
// An assertion is a check you leave switched on in development: if the
// condition is false, the program stops and tells you where. Release builds
// compile WP_ASSERT away unless WAYPOINT_ENABLE_ASSERTS is defined.
#pragma once

#include <cstdio>
#include <cstdlib>

namespace waypoint::detail {
[[noreturn]] inline void assertionFailed(const char* expression, const char* file, int line) {
    std::fprintf(stderr, "Waypoint assertion failed: (%s)\n  at %s:%d\n", expression, file, line);
    std::fflush(stderr);
    std::abort();
}
}  // namespace waypoint::detail

#if defined(WAYPOINT_ENABLE_ASSERTS) || defined(WAYPOINT_DEBUG)
#define WP_ASSERT(expression)                                                      \
    do {                                                                           \
        if (!(expression)) {                                                       \
            ::waypoint::detail::assertionFailed(#expression, __FILE__, __LINE__);  \
        }                                                                          \
    } while (false)
#define WP_ASSERT_MSG(expression, message)                                         \
    do {                                                                           \
        if (!(expression)) {                                                       \
            std::fprintf(stderr, "Waypoint assertion failed: (%s)\n  %s\n  at %s:%d\n", \
                         #expression, (message), __FILE__, __LINE__);              \
            std::fflush(stderr);                                                   \
            std::abort();                                                          \
        }                                                                          \
    } while (false)
#else
#define WP_ASSERT(expression) ((void)sizeof(!(expression)))
#define WP_ASSERT_MSG(expression, message) ((void)sizeof(!(expression)), (void)(message))
#endif

// Always-on check used for programmer errors that must never be silent.
#define WP_VERIFY(expression) WP_ASSERT_MSG(expression, "verification failed")

#if defined(_MSC_VER)
#define WP_DEBUG_BREAK() __debugbreak()
#elif defined(__GNUC__) || defined(__clang__)
#define WP_DEBUG_BREAK() __builtin_trap()
#else
#define WP_DEBUG_BREAK() std::abort()
#endif
