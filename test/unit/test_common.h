#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#define CATCH_CONFIG_NO_COUNTER 1
// IWYU pragma: begin_exports
#include "catch2/catch_test_macros.hpp"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <type_traits>
// IWYU pragma: end_exports

// Catch2 prints uint8_t comparisons as ascii chars. If I cast them to std::byte first then it will print them as a 8
// bit num.
#define CHECK_BYTES_EQ(byte1, byte2)   CHECK(std::byte(byte1) == std::byte(byte2));
#define REQUIRE_BYTES_EQ(byte1, byte2) REQUIRE(std::byte(byte1) == std::byte(byte2));

static inline void assert_int_eq(int v1, int v2) {
  CHECKED_IF(v1 != v2) { FAIL(); }
}
static inline void assert_uint8_eq_hex(uint8_t v1, uint8_t v2) {
  CHECKED_IF(v1 != v2) { FAIL(); }
}
static inline void assert_uint16_eq_hex(uint16_t v1, uint16_t v2) {
  CHECKED_IF(v1 != v2) { FAIL(); }
}
static inline void assert_str_eq(char *v1, char *v2) {
  CHECKED_IF(strcmp(v1, v2)) { FAIL("v1:\n" << v1 << "\nv2:\n" << v2); }
}
template <typename T1, typename T2> void assert_eq(T1 v1, T2 v2) {
  if constexpr (std::is_same_v<T1, uint8_t>) {
    return assert_uint8_eq_hex(v1, (T1)v2);
  } else if constexpr (std::is_same_v<T1, unsigned char>) {
    return assert_uint8_eq_hex(v1, (T1)v2);
  } else if constexpr (std::is_same_v<T1, uint16_t>) {
    return assert_uint16_eq_hex(v1, (T1)v2);
  } else if constexpr (std::is_same_v<T1, uint32_t>) {
    return assert_int_eq(v1, (T1)v2);
  } else if constexpr (std::is_same_v<T1, int>) {
    return assert_int_eq(v1, (T1)v2);
  } else if constexpr (std::is_same_v<T1, char *>) {
    return assert_str_eq(v1, (T1)v2);
  } else {
    static_assert(false, "Unsupported type for assert_eq");
  }
}
#endif
