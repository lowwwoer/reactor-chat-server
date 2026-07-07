// 最小 HTTP/1.1 服务器（附录 A 拉伸项）：直接复用 net/ 网络库（TcpServer/TcpConnection/
// Buffer/EventLoop），业务层只做「攒到完整请求头 → 解析请求行 → 回一个固定页面」，
// 证明这套 epoll/主从 Reactor 网络库不绑定聊天协议、可复用于别的字节流协议。
//
// 简化取舍：只解析请求行（方法 + 路径），忽略请求体；每个请求回完即 Connection: close
// 半关闭（省掉 keep-alive 状态机）。监听端口取环境变量 HTTP_PORT（默认 8080）。
#include "net/TcpServer.h"
#include "net/EventLoop.h"
#include "base/Buffer.h"
#include "base/InetAddress.h"
#include "base/Logging.h"

#include <cstdlib>
#include <sstream>
#include <string>

// 把 & < > 转义，避免把请求路径原样嵌进 HTML（顺手防一下反射式 XSS）。
static std::string htmlEscape(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      default:  out += c;
    }
  }
  return out;
}

static void onMessage(const TcpConnectionPtr& conn, Buffer* buf) {
  // 请求头以空行 \r\n\r\n 结束；没收全就先返回，等下次可读事件继续攒（半包处理）。
  std::string data(buf->peek(), buf->readableBytes());
  if (data.find("\r\n\r\n") == std::string::npos) return;
  buf->retrieveAll();  // demo：一个连接只处理一个请求，收到完整头即消费掉整个缓冲

  // 请求行：METHOD SP PATH SP HTTP/x.y。只取前两段，版本忽略。
  std::string method = "?", path = "/";
  {
    std::istringstream iss(data.substr(0, data.find("\r\n")));
    iss >> method >> path;
  }

  std::string body =
      "<!doctype html><html><head><meta charset=\"utf-8\">"
      "<title>netlib http demo</title></head><body>"
      "<h1>netlib HTTP demo</h1>"
      "<p>You requested: <b>" + htmlEscape(method) + "</b> "
      "<code>" + htmlEscape(path) + "</code></p>"
      "<p>复用同一套 epoll / 主从 Reactor 网络库（net/）实现。</p>"
      "</body></html>\n";

  std::string resp =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "Content-Length: " + std::to_string(body.size()) + "\r\n"
      "Connection: close\r\n"
      "\r\n" + body;
  conn->send(resp);
  conn->shutdown();  // 回完即半关闭：输出缓冲 flush 完才真正关，对端收齐正文
}

int main() {
  int port = 8080;
  if (const char* p = std::getenv("HTTP_PORT"); p && std::atoi(p) > 0) port = std::atoi(p);

  EventLoop loop;
  TcpServer server(&loop, InetAddress(static_cast<uint16_t>(port)), "http");
  server.setThreadNum(2);
  server.setMessageCallback(onMessage);
  LOG_INFO("http_server listening on %d", port);
  server.start();
  loop.loop();
}
