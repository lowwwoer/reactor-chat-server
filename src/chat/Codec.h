#pragma once
// Codec：聊天用的「行协议」编解码。一行（以 '\n' 结尾）就是一条消息。
// 网络层给上来的是字节流，可能粘包/半包；takeLines 负责切出完整行、把半行留在缓冲里。
#include <string>
#include <vector>

class Buffer;

namespace codec {

// 从缓冲取出所有完整行（不含 '\n'）；末尾没有 '\n' 的半行保留在 buf 里。
std::vector<std::string> takeLines(Buffer* buf);

// 拼出一条广播消息： "[room] nick: text\n"
std::string formatBroadcast(const std::string& room, const std::string& nick,
                            const std::string& text);

}  // namespace codec
