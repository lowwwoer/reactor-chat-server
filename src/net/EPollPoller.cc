#include "net/EPollPoller.h"
#include "net/Channel.h"
#include "base/Logging.h"

#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <cerrno>

EPollPoller::EPollPoller(EventLoop*)
    : epollfd_(::epoll_create1(EPOLL_CLOEXEC)), events_(16) {
  if (epollfd_ < 0) {
    LOG_SYSERR("epoll_create1");
    abort();
  }
}

EPollPoller::~EPollPoller() { ::close(epollfd_); }

void EPollPoller::poll(int timeoutMs, std::vector<Channel*>* activeChannels) {
  int n = ::epoll_wait(epollfd_, events_.data(),
                       static_cast<int>(events_.size()), timeoutMs);
  if (n < 0) {
    if (errno != EINTR) LOG_SYSERR("epoll_wait");
    return;
  }
  for (int i = 0; i < n; ++i) {
    Channel* ch = static_cast<Channel*>(events_[i].data.ptr);
    ch->set_revents(events_[i].events);
    activeChannels->push_back(ch);
  }
  // 就绪事件填满了数组，说明可能还有更多，下次扩容。
  if (n == static_cast<int>(events_.size())) events_.resize(events_.size() * 2);
}

void EPollPoller::update(int op, Channel* ch) {
  epoll_event ev;
  std::memset(&ev, 0, sizeof ev);
  ev.events = ch->events();
  ev.data.ptr = ch;  // 关键：把 Channel* 塞进 epoll，poll 时原样取回
  if (::epoll_ctl(epollfd_, op, ch->fd(), &ev) < 0) LOG_SYSERR("epoll_ctl");
}

void EPollPoller::updateChannel(Channel* ch) {
  const int idx = ch->index();
  if (idx == kNew || idx == kDeleted) {
    channels_[ch->fd()] = ch;
    ch->set_index(kAdded);
    update(EPOLL_CTL_ADD, ch);
  } else {
    if (ch->events() == 0) {  // 不再关注任何事件 → 从 epoll 删除
      update(EPOLL_CTL_DEL, ch);
      ch->set_index(kDeleted);
    } else {
      update(EPOLL_CTL_MOD, ch);
    }
  }
}

void EPollPoller::removeChannel(Channel* ch) {
  channels_.erase(ch->fd());
  if (ch->index() == kAdded) update(EPOLL_CTL_DEL, ch);
  ch->set_index(kNew);
}
