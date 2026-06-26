#pragma once
// 对 epoll 的封装：epoll_create / epoll_ctl / epoll_wait。
// poll() 返回有事件的 Channel 列表，交给 EventLoop 逐个 handleEvent。
#include <vector>
#include <unordered_map>
#include <sys/epoll.h>

class Channel;
class EventLoop;

class EPollPoller {
 public:
  explicit EPollPoller(EventLoop* loop);
  ~EPollPoller();

  // 阻塞至多 timeoutMs，把就绪的 Channel 填进 activeChannels。
  void poll(int timeoutMs, std::vector<Channel*>* activeChannels);

  void updateChannel(Channel* ch);  // 新增/修改 fd 在 epoll 中的关注事件
  void removeChannel(Channel* ch);  // 从 epoll 中移除 fd

 private:
  void update(int operation, Channel* ch);  // 实际的 epoll_ctl

  // Channel::index_ 的三种状态：从未加入 / 已在 epoll 中 / 曾加入但已删
  static const int kNew = -1;
  static const int kAdded = 1;
  static const int kDeleted = 2;

  int epollfd_;
  std::vector<epoll_event> events_;             // epoll_wait 的输出数组，会按需扩容
  std::unordered_map<int, Channel*> channels_;  // fd → Channel
};
