#include "base/InetAddress.h"
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>

InetAddress::InetAddress(uint16_t port, std::string ip) {
  std::memset(&addr_, 0, sizeof addr_);
  addr_.sin_family = AF_INET;
  addr_.sin_port = htons(port);
  ::inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr);
}

std::string InetAddress::toIpPort() const {
  char buf[64] = {0};
  ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
  size_t end = std::strlen(buf);
  uint16_t port = ntohs(addr_.sin_port);
  std::snprintf(buf + end, sizeof buf - end, ":%u", port);
  return buf;
}
