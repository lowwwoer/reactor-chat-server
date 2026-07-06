import os, socket, subprocess, time
# 缺陷回归：并发 /nick（属主线程写 session->nick）+ /who（members() 跨线程读同一
# nick）。修复前写端不持 RoomManager 锁，是数据竞争 —— 用 TSan 构建跑本脚本会打
# "WARNING: ThreadSanitizer: data race"。本脚本捕获服务器 stderr 断言其不出现：
#   CHAT_BIN=./build-tsan/chat_server python3 tests/integration/nick_who_race_it.py
p = subprocess.Popen([os.environ.get("CHAT_BIN", "./build/chat_server")],
                     stderr=subprocess.PIPE)
time.sleep(0.5)
def conn():
    s = socket.create_connection(("127.0.0.1", 9000)); s.settimeout(2.0); return s
try:
    a, b = conn(), conn()   # chat_server setThreadNum(4) 轮询分发 → 两连接在不同 IO 线程
    a.sendall(b"/nick alice\n/join lobby\n")
    b.sendall(b"/nick bob\n/join lobby\n")
    time.sleep(0.3)
    for i in range(300):    # 两个 IO 线程同时高频「写 alice 的 nick」/「读全房间 nick」
        a.sendall(("/nick alice%d\n" % i).encode())
        b.sendall(b"/who\n")
    time.sleep(0.5)         # 留时间让两侧队列处理完（竞争窗口在此期间）
finally:
    p.terminate()
err = p.communicate(timeout=10)[1] or b""
assert b"ThreadSanitizer" not in err, err.decode(errors="replace")[:2000]
print("nick_who_race_it OK")
