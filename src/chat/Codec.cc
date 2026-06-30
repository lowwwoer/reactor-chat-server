#include "chat/Codec.h"
#include "base/Buffer.h"

namespace codec {

std::vector<std::string> takeLines(Buffer* buf) {
  std::vector<std::string> lines;
  while (const char* eol = buf->findEOL()) {
    size_t len = static_cast<size_t>(eol - buf->peek());  // 行内容长度（不含 '\n'）
    lines.push_back(buf->retrieveAsString(len));
    buf->retrieve(1);  // 吃掉行尾的 '\n'
  }
  return lines;  // 没有 '\n' 的半行留在 buf 里，等后续数据拼齐
}

std::string formatBroadcast(const std::string& room, const std::string& nick,
                            const std::string& text) {
  return "[" + room + "] " + nick + ": " + text + "\n";
}

}  // namespace codec
