#include "chat/RoomManager.h"

#include <memory>

void RoomManager::join(const std::string& room, const TcpConnectionPtr& conn) {
  auto session = std::static_pointer_cast<Session>(conn->getContext());
  if (!session->room.empty()) {
    rooms_[session->room].erase(conn);  // 先离开旧房间
  }
  session->room = room;
  rooms_[room].insert(conn);
}

void RoomManager::leave(const TcpConnectionPtr& conn) {
  auto session = std::static_pointer_cast<Session>(conn->getContext());
  if (session && !session->room.empty()) {
    rooms_[session->room].erase(conn);
  }
}

void RoomManager::broadcast(const std::string& room, const std::string& line) {
  auto it = rooms_.find(room);
  if (it == rooms_.end()) return;
  for (const auto& conn : it->second) conn->send(line);
}

std::vector<std::string> RoomManager::members(const std::string& room) {
  std::vector<std::string> names;
  auto it = rooms_.find(room);
  if (it == rooms_.end()) return names;
  for (const auto& conn : it->second) {
    auto session = std::static_pointer_cast<Session>(conn->getContext());
    if (session) names.push_back(session->nick);
  }
  return names;
}
