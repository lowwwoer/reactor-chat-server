cmake_minimum_required(VERSION 3.16)
project(tcp_chat_server CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -g)

# 可选 sanitizer： cmake -S . -B build-tsan -DSAN=thread   （或 -DSAN=address）
if(SAN)
  add_compile_options(-fsanitize=${SAN} -fno-omit-frame-pointer)
  add_link_options(-fsanitize=${SAN})
endif()

include_directories(src)

# ──────────────────────────────────────────────────────────────────
# netlib：网络库。从 Task 3 起每写一个 .cc，就把它加进下面 add_library
# 的源文件列表，并取消本块注释。（Task 1 只构建 hello，故先整体注释。）
# ──────────────────────────────────────────────────────────────────
# find_package(Threads REQUIRED)
# add_library(netlib
#   src/base/InetAddress.cc
#   src/base/Socket.cc
#   src/base/Buffer.cc
#   src/net/Channel.cc
#   src/net/EPollPoller.cc
#   src/net/EventLoop.cc
#   src/net/EventLoopThread.cc
#   src/net/EventLoopThreadPool.cc
#   src/net/Acceptor.cc
#   src/net/TcpConnection.cc
#   src/net/TcpServer.cc
#   src/chat/Codec.cc
#   src/chat/RoomManager.cc
#   src/chat/ChatServer.cc
# )
# target_link_libraries(netlib PUBLIC Threads::Threads)

# 工具链自检可执行（Task 1）
add_executable(hello examples/hello.cc)

# ──────────────────────────────────────────────────────────────────
# 测试：对应 Task 完成后取消注释。
# ──────────────────────────────────────────────────────────────────
enable_testing()
# add_executable(buffer_test    tests/buffer_test.cc);    target_link_libraries(buffer_test netlib)
# add_test(NAME buffer_test    COMMAND buffer_test)
# add_executable(codec_test     tests/codec_test.cc);     target_link_libraries(codec_test netlib)
# add_test(NAME codec_test     COMMAND codec_test)
# add_executable(eventloop_test tests/eventloop_test.cc); target_link_libraries(eventloop_test netlib)
# add_test(NAME eventloop_test COMMAND eventloop_test)

# ──────────────────────────────────────────────────────────────────
# 示例与压测：对应 Task 取消注释。
# ──────────────────────────────────────────────────────────────────
# add_executable(echo_server  examples/echo_server.cc); target_link_libraries(echo_server netlib)
# add_executable(chat_server  examples/chat_server.cc); target_link_libraries(chat_server netlib)
# add_executable(bench_client bench/bench_client.cc);   target_link_libraries(bench_client netlib)
