#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" [[noreturn]] void hrd_array_bounds_error(int64_t index,
                                                    uint64_t length) {
  std::fprintf(stderr,
               "HRD runtime error: array index out of bounds "
               "(index: %lld, length: %llu)\n",
               static_cast<long long>(index),
               static_cast<unsigned long long>(length));

  std::abort();
}