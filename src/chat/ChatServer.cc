#include "chat/ChatServer.h"
#include "chat/Codec.h"
#include "net/EventLoop.h"
#include "base/Buffer.h"

#include <memory>
#include <string>

ChatServer::ChatServer(EventLoop* loop, const InetAddress& addr)
    : server_(loop, addr, "chat") {
  server_.setConnectionCallback(
      [this](const TcpConnectionPtr& conn) { onConnection(conn); });
  server_.setMessageCallback(
      [this](const TcpConnectionPtr& conn, Buffer* buf) { onMessage(conn, buf); });
}

void ChatServer::setThreadNum(int n) { server_.setThreadNum(n); }

void ChatServer::start() { server_.start(); }

void ChatServer::onConnection(const TcpConnectionPtr& conn) {
  if (conn->connected()) {
    conn->setContext(std::make_shared<Session>());
    conn->send("欢迎！用 /nick <名> 设昵称，/join <房间> 进房，/who 看在线，/quit 退出。\n");
  } else {
    rooms_.leave(conn);  // 断开时把它从所在房间摘掉
  }
}

void ChatServer::onMessage(const TcpConnectionPtr& conn, Buffer* buf) {
  auto session = std::static_pointer_cast<Session>(conn->getContext());
  for (const std::string& line : codec::takeLines(buf)) {
    if (line.rfind("/nick ", 0) == 0) {
      const std::string nick = line.substr(6);
      rooms_.setNick(conn, nick);  // members() 会跨线程读 nick，写必须持 RoomManager 锁
      conn->send("昵称已设为 " + nick + "\n");
    } else if (line.rfind("/join ", 0) == 0) {
      const std::string room = line.substr(6);
      if (room.empty()) {  // 空串是「未进房」哨兵，进了 rooms_[""] 断开时 leave 摘不掉
        conn->send("房间名不能为空。\n");
        continue;
      }
      rooms_.join(room, conn);
      rooms_.broadcast(
          room, codec::formatBroadcast(room, "系统", session->nick + " 加入了房间"));
    } else if (line == "/who") {
      std::string list;
      for (const std::string& nick : rooms_.members(session->room)) list += nick + " ";
      conn->send("在线: " + list + "\n");
    } else if (line == "/quit") {
      conn->shutdown();
    } else if (session->room.empty()) {
      conn->send("请先 /join 一个房间再发言。\n");
    } else {
      rooms_.broadcast(session->room,
                       codec::formatBroadcast(session->room, session->nick, line));
    }
  }
}
