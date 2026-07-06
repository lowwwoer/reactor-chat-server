#include "chat/RoomManager.h"

#include <memory>

void RoomManager::setNick(const TcpConnectionPtr& conn, const std::string& nick) {
  auto session = std::static_pointer_cast<Session>(conn->getContext());
  std::lock_guard<std::mutex> lk(mutex_);
  session->nick = nick;
}

void RoomManager::join(const std::string& room, const TcpConnectionPtr& conn) {
  auto session = std::static_pointer_cast<Session>(conn->getContext());
  std::lock_guard<std::mutex> lk(mutex_);
  if (!session->room.empty()) {
    rooms_[session->room].erase(conn);  // 先离开旧房间
  }
  session->room = room;
  rooms_[room].insert(conn);
}

void RoomManager::leave(const TcpConnectionPtr& conn) {
  auto session = std::static_pointer_cast<Session>(conn->getContext());
  std::lock_guard<std::mutex> lk(mutex_);
  if (session && !session->room.empty()) {
    rooms_[session->room].erase(conn);
  }
}

void RoomManager::broadcast(const std::string& room, const std::string& line) {
  std::lock_guard<std::mutex> lk(mutex_);
  auto it = rooms_.find(room);
  if (it == rooms_.end()) return;
  // 持锁遍历可接受：send 非阻塞（跨线程时只是投递任务，见 TcpConnection::send）。
  for (const auto& conn : it->second) conn->send(line);
}

std::vector<std::string> RoomManager::members(const std::string& room) {
  std::vector<std::string> names;
  std::lock_guard<std::mutex> lk(mutex_);
  auto it = rooms_.find(room);
  if (it == rooms_.end()) return names;
  for (const auto& conn : it->second) {
    auto session = std::static_pointer_cast<Session>(conn->getContext());
    if (session) names.push_back(session->nick);
  }
  return names;
}
