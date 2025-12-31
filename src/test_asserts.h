#include "common.h"

#define LOC               __FILE__, __LINE__, __func__

#define assert_eq(v1, v2) assert_int_eq(LOC, v1, v2)

static void assert_int_eq(const char *fname, int lineno, const char *fxname,
                          int v1, int v2) {
  if (v1 != v2) {
    fprintf(stderr,"test assertion failed:\n");
    fprintf(stderr, "%s@%d - %s(): v1 (%d) != v2 (%d)\n", fname, lineno, fxname,
            v1, v2);
    abort();
  }
}
