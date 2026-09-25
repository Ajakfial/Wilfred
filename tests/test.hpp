#pragma once

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace wilfred::test {

inline int g_fails = 0;
inline int g_passes = 0;

inline void check(bool cond, const char* expr, const char* file, int line) {
  if (cond) {
    ++g_passes;
  } else {
    ++g_fails;
    std::cerr << "FAIL " << file << ":" << line << "  " << expr << "\n";
  }
}

}  // namespace wilfred::test

#define CHECK(expr) ::wilfred::test::check(static_cast<bool>(expr), #expr, __FILE__, __LINE__)
#define CHECK_EQ(a, b) \
  ::wilfred::test::check((a) == (b), #a " == " #b, __FILE__, __LINE__)
#define CHECK_NEAR(a, b, eps)                                                       \
  ::wilfred::test::check(std::abs(static_cast<double>(a) - static_cast<double>(b)) < \
                             (eps),                                                 \
                         #a " ~= " #b, __FILE__, __LINE__)

void test_yaml();
void test_config();
void test_tokenizer();
void test_fuzzy();
void test_rank();
void test_filter();
void test_math();
void test_classify();
void test_index();
void test_paths();
void test_history();
void test_apps();
void test_protocol();
void test_search_extras();
