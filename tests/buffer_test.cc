#include "base/Buffer.h"
#include "../tests/test_main.h"
#include <string>

int main(){
  Buffer b;
  CHECK_EQ(b.readableBytes(), 0u);

  b.append(std::string("hello"));
  CHECK_EQ(b.readableBytes(), 5u);
  CHECK_EQ(std::string(b.peek(), 5), std::string("hello"));

  std::string s = b.retrieveAsString(2);          // 取出 "he"
  CHECK_EQ(s, std::string("he"));
  CHECK_EQ(b.readableBytes(), 3u);
  CHECK_EQ(b.retrieveAllAsString(), std::string("llo"));
  CHECK_EQ(b.readableBytes(), 0u);

  b.append(std::string("ab\ncd"));                // 行查找
  const char* eol = b.findEOL();
  CHECK(eol != nullptr);
  CHECK_EQ(static_cast<int>(eol - b.peek()), 2);  // '\n' 在偏移 2

  TEST_SUMMARY();
}