#pragma once

#include <cassert>

// original from thorin2

namespace sql {

// see https://stackoverflow.com/a/65258501
#ifdef __GNUC__ // GCC 4.8+, Clang, Intel and other compilers compatible with GCC (-std=c++0x or above)
[[noreturn]] inline __attribute__((always_inline)) void unreachable() {
    assert(false);
    __builtin_unreachable();
}
#elif defined(_MSC_VER) // MSVC
[[noreturn]] __forceinline void unreachable() {
    assert(false);
    __assume(false);
}
#else                   // ???
inline void unreachable() { assert(false); }
#endif

#if (defined(__clang__) || defined(__GNUC__)) && (defined(__x86_64__) || defined(__i386__))
inline void breakpoint() { asm("int3"); }
#else
inline void breakpoint() {
    volatile int* p = nullptr;
    *p              = 42;
}
#endif

} // namespace sql
