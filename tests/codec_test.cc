#include "chat/Codec.h"
#include "base/Buffer.h"
#include "../tests/test_main.h"
#include <string>

int main() {
  Buffer b;
  b.append(std::string("a\nbc\nde"));  // "de" 是半行
  auto lines = codec::takeLines(&b);
  CHECK_EQ(lines.size(), 2u);
  CHECK_EQ(lines[0], std::string("a"));
  CHECK_EQ(lines[1], std::string("bc"));
  CHECK_EQ(b.retrieveAllAsString(), std::string("de"));  // 半行保留

  CHECK_EQ(codec::formatBroadcast("lobby", "tom", "hi"),
           std::string("[lobby] tom: hi\n"));
  TEST_SUMMARY();
}
