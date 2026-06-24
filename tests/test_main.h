#pragma once
// 极简测试断言：避免引入 GoogleTest 等重型依赖。
// 用法：在测试 main() 里用 CHECK/CHECK_EQ，结尾调 TEST_SUMMARY()。
#include <cstdio>
#include <cstdlib>

inline int g_checks = 0, g_fails = 0;

#define CHECK(cond)                                                          \
  do {                                                                       \
    ++g_checks;                                                              \
    if (!(cond)) {                                                           \
      ++g_fails;                                                             \
      std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);   \
    }                                                                        \
  } while (0)

#define CHECK_EQ(a, b)                                                       \
  do {                                                                       \
    ++g_checks;                                                              \
    if (!((a) == (b))) {                                                     \
      ++g_fails;                                                             \
      std::fprintf(stderr, "FAIL %s:%d  %s == %s\n", __FILE__, __LINE__,     \
                   #a, #b);                                                  \
    }                                                                        \
  } while (0)

#define TEST_SUMMARY()                                                      \
  do {                                                                       \
    std::fprintf(stderr, "%d checks, %d failures\n", g_checks, g_fails);     \
    return g_fails == 0 ? 0 : 1;                                             \
  } while (0)
