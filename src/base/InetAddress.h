#pragma once
// IPv4 地址（sockaddr_in）的薄封装：构造、取出 "ip:port" 字符串、拿底层指针。
#include <netinet/in.h>
#include <string>
#include <cstdint>

class InetAddress {
 public:
  explicit InetAddress(uint16_t port, std::string ip = "0.0.0.0");
  explicit InetAddress(const sockaddr_in& addr) : addr_(addr) {}

  std::string toIpPort() const;
  const sockaddr_in* sockAddr() const { return &addr_; }

 private:
  sockaddr_in addr_;
};
