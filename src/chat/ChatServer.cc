#include "chat/ChatServer.h"
#include "chat/Codec.h"
#include "net/EventLoop.h"
#include "base/Buffer.h"

#include <chrono>
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

void ChatServer::setEdgeTriggered(bool on) { server_.setEdgeTriggered(on); }

void ChatServer::start() { server_.start(); }

void ChatServer::armIdleTimer(const TcpConnectionPtr& conn, double delay) {
  // 捕获 weak_ptr 而非 shared_ptr：定时器不该续着一条已断开的连接的命；
  // 每次续约都新建一个 one-shot 定时器（续约链），连接断开后 lock 失败即自然终止。
  std::weak_ptr<TcpConnection> weak = conn;
  conn->getLoop()->runAfter(delay, [this, weak] {
    TcpConnectionPtr c = weak.lock();
    if (!c || !c->connected()) return;  // 已断开：停止续约
    auto session = std::static_pointer_cast<Session>(c->getContext());
    const double idle = std::chrono::duration<double>(
                            std::chrono::steady_clock::now() - session->lastActive)
                            .count();
    if (idle >= idleTimeout_) {
      c->send("⏱ 空闲超时，断开连接。\n");
      c->shutdown();  // 半关闭：欢送语 flush 完再关，对端读到 EOF
    } else {
      armIdleTimer(c, idleTimeout_ - idle);  // 还活跃：精确续约到「上次活跃 + 超时」时刻
    }
  });
}

void ChatServer::onConnection(const TcpConnectionPtr& conn) {
  if (conn->connected()) {
    conn->setContext(std::make_shared<Session>());
    conn->send("欢迎！用 /nick <名> 设昵称，/join <房间> 进房，/who 看在线，/quit 退出。\n");
    if (idleTimeout_ > 0) armIdleTimer(conn, idleTimeout_);
  } else {
    rooms_.leave(conn);  // 断开时把它从所在房间摘掉
  }
}

void ChatServer::onMessage(const TcpConnectionPtr& conn, Buffer* buf) {
  auto session = std::static_pointer_cast<Session>(conn->getContext());
  session->lastActive = std::chrono::steady_clock::now();  // 有输入即刷新活跃时间
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
