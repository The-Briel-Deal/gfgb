#ifndef TEST_ASSERTS_H
#define TEST_ASSERTS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOC __FILE__, __LINE__, __func__

#ifdef __cplusplus
template <typename T1, typename T2> void assert_eq(T1 v1, T2 v2) {
  if constexpr (std::is_same_v<T1, uint8_t>) return assert_uint8_eq_hex(LOC, v1, (T1)v2);
  if constexpr (std::is_same_v<T1, uint16_t>) return assert_uint16_eq_hex(LOC, v1, (T1)v2);
  if constexpr (std::is_same_v<T1, uint32_t>) return assert_int_eq(LOC, v1, (T1)v2);
  if constexpr (std::is_same_v<T1, int>) return assert_int_eq(LOC, v1, (T1)v2);
  if constexpr (std::is_same_v<T1, char *>) return assert_str_eq(LOC, v1, (T1)v2);

  static_assert(false, "Unsupported type for assert_eq");
}

#else
#define assert_eq(v1, v2)                                                                                              \
  _Generic((v1),                                                                                                       \
      uint8_t: assert_uint8_eq_hex,                                                                                    \
      uint16_t: assert_uint16_eq_hex,                                                                                  \
      uint32_t: assert_int_eq,                                                                                         \
      int: assert_int_eq,                                                                                              \
      char *: assert_str_eq)(LOC, v1, v2)
#endif

static inline void assert_int_eq(const char *fname, int lineno, const char *fxname, int v1, int v2) {
  if (v1 != v2) {
    fprintf(stderr, "test assertion failed:\n");
    fprintf(stderr, "%s@%d - %s(): v1 (%d) != v2 (%d)\n", fname, lineno, fxname, v1, v2);
    abort();
  }
}
static inline void assert_uint8_eq_hex(const char *fname, int lineno, const char *fxname, uint8_t v1, uint8_t v2) {
  if (v1 != v2) {
    fprintf(stderr, "test assertion failed:\n");
    fprintf(stderr, "%s@%d - %s(): v1 (0x%.2x) != v2 (0x%.2x)\n", fname, lineno, fxname, v1, v2);
    abort();
  }
}
static inline void assert_uint16_eq_hex(const char *fname, int lineno, const char *fxname, uint16_t v1, uint16_t v2) {
  if (v1 != v2) {
    fprintf(stderr, "test assertion failed:\n");
    fprintf(stderr, "%s@%d - %s(): v1 (0x%.4x) != v2 (0x%.4x)\n", fname, lineno, fxname, v1, v2);
    abort();
  }
}
static inline void assert_str_eq(const char *fname, int lineno, const char *fxname, char *v1, char *v2) {
  if (strcmp(v1, v2)) {
    fprintf(stderr, "test assertion failed:\n");
    fprintf(stderr, "%s@%d - %s(): v1 (%s) != v2 (%s)\n", fname, lineno, fxname, v1, v2);
    abort();
  }
}

#endif
