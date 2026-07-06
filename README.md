# 多线程 TCP 聊天服务器（C++17 · epoll · 主从 Reactor）

用 C++17 从零实现的多线程 TCP 聊天服务器：**epoll(LT) + 主从 Reactor（one-loop-per-thread）**，
支持多房间广播；跨线程收发用 `runInLoop` + eventfd 唤醒保证线程安全，并通过
**ThreadSanitizer / AddressSanitizer** 验证零竞争、零内存错误。

关键词：`epoll` · `主从 Reactor` · `one-loop-per-thread` · `C++17/RAII/智能指针` · `TSan/ASan`

## 构建与运行（Docker）

平台为 Linux/epoll，macOS 上用 Docker 编译运行：

```bash
./docker/run.sh                # 构建 netlab 镜像并进入容器（挂载源码到 /work，映射 9000 端口）
# 容器内：
cmake -S . -B build && cmake --build build -j
./build/chat_server            # 监听 9000
```

另开终端连上聊天（可开多个互聊）：

```bash
nc 127.0.0.1 9000
/nick tom        # 设昵称
/join lobby      # 进房间
hello everyone   # 普通文本广播到当前房间
/who             # 看房间在线
/quit            # 退出（服务器 flush 完再半关闭，不丢尾部数据）
```

## 架构

```
                    ┌──────────────────────────────────────────────┐
   client ──┐       │                 TcpServer                    │
   client ──┼─TCP──▶│  ┌──────────────┐                            │
   client ──┘       │  │ Main Reactor │  Acceptor(listen fd)       │
                    │  │  (主线程)     │  accept 后轮询分发 ─────┐   │
                    │  └──────────────┘                        │   │
                    │  ┌──────────────┐ ┌──────────────┐ ┌─────▼────────┐
                    │  │ Sub Reactor 0│ │ Sub Reactor 1│ │ Sub Reactor N│
                    │  │ EventLoop +  │ │ EventLoop +  │ │ EventLoop +  │
                    │  │ epoll (线程0)│ │ epoll (线程1)│ │ epoll (线程N)│
                    │  │ TcpConn ...  │ │ TcpConn ...  │ │ TcpConn ...  │
                    │  └──────────────┘ └──────────────┘ └──────────────┘
                    │    共享: RoomManager(房间 → 连接集合, 互斥锁保护)     │
                    └──────────────────────────────────────────────┘
```

- **Main Reactor（主线程）**：只 `accept`，轮询分发新连接给 Sub Reactor。
- **Sub Reactor × N**：每线程一个 `EventLoop`（一个 epoll），负责名下连接的全部读写
  与业务回调——**单条连接内部天然无锁**。
- **唯一共享可变状态**：`RoomManager` 房间表，一把 `std::mutex` 保护。
- 分层：`base`（Buffer/Socket/InetAddress）→ `net`（Channel/EPollPoller/EventLoop/
  TcpConnection/TcpServer）→ `chat`（Codec/RoomManager/ChatServer），业务不碰 epoll。

## 压测

压测客户端（`bench/bench_client.cc`）：每连接一线程闭环收发，消息内嵌
`steady_clock` 纳秒时间戳，统计**广播投递吞吐**（送达消息数/秒）与投递延迟 p50/p99：

```bash
./build/chat_server &
./build/bench_client --conns 200 --seconds 10 --rooms 20
```

环境：colima VM（Apple Silicon，6 vCPU，Ubuntu 22.04），服务器与压测端同机 loopback，
200 并发连接 × 10 秒，消息约 35 字节。

**20 房间（每房 10 人，扇出 10）——多 Reactor 并行生效，吞吐随线程数扩展：**

| IO 线程数 | 投递吞吐 (msg/s) | p50 | p99 |
|---:|---:|---:|---:|
| 1 | 51,530 | 43.6 ms | 51.3 ms |
| 2 | 57,701 | 42.3 ms | 50.4 ms |
| 4 | 71,725 | 40.9 ms | 46.8 ms |
| 8 | **93,388** | 39.2 ms | **47.1 ms** |

**单房间 200 人（扇出 200）——压满广播路径的极端场景：**

| IO 线程数 | 投递吞吐 (msg/s) | p50 | p99 |
|---:|---:|---:|---:|
| 1 | 1,188,516 | 32.1 ms | 64.9 ms |
| 2 | 1,299,448 | 48.4 ms | 186.6 ms |
| 4 | **1,434,070** | 42.8 ms | 109.2 ms |
| 8 | 1,355,298 | 48.6 ms | 221.5 ms |

结论：多房间负载下吞吐随 IO 线程数单调上升（1→8 线程 +81%，p99 反而下降）；
单房间极端扇出时峰值投递 **143 万条/秒**，但继续加线程收益消失——瓶颈转移到
单房间广播的全局锁串行化与跨线程任务投递开销，这也是 6 vCPU 上
压测端 200 线程与服务器同机抢核的合成结果。

## 测试

```bash
# 单测（Buffer/Codec/EventLoop 跨线程投递）
./build/buffer_test && ./build/codec_test && ./build/eventloop_test
# 集成（echo / 多客户端聊天 / 大消息半关闭）
python3 tests/integration/echo_it.py
python3 tests/integration/chat_it.py
python3 tests/integration/halfclose_it.py
# sanitizer 构建（CHAT_BIN 切换被测二进制）
cmake -S . -B build-tsan -DSAN=thread && cmake --build build-tsan -j
CHAT_BIN=./build-tsan/chat_server python3 tests/integration/chat_it.py
```

> Docker 里跑 sanitizer 需 `--security-opt seccomp=unconfined`。
