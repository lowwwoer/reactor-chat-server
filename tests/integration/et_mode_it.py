import os, socket, subprocess, time
# ET（边沿触发）模式集成测试：CHAT_ET=1 启动服务器（附录 A 拉伸项）。
# ET 下事件只在「从无到有」时通知一次，服务器必须循环读/写到 EAGAIN；
# 用 2MB 单行广播压出「一次通知取不完」的场景——漏循环会表现为收不齐/卡死。
env = dict(os.environ, CHAT_ET="1")
p = subprocess.Popen([os.environ.get("CHAT_BIN", "./build/chat_server")],
                     env=env, stderr=subprocess.PIPE)
time.sleep(0.5)
def conn():
    s = socket.create_connection(("127.0.0.1", 9000)); s.settimeout(2.0); return s
def read_exact(s, expected):
    got = bytearray()
    while len(got) < len(expected):
        chunk = s.recv(65536)
        assert chunk, f"对端提前关闭，已收 {len(got)}/{len(expected)} 字节"
        got.extend(chunk)
    return bytes(got)
try:
    a, b = conn(), conn()
    for s, n in ((a, "alice"), (b, "bob")):
        s.sendall(f"/nick {n}\n/join lobby\n".encode())
    time.sleep(0.3)
    for s in (a, b):  # 清掉欢迎/昵称/加入提示
        try:
            while True: s.recv(4096)
        except socket.timeout: pass
    # 2MB 单行：远超内核收发缓冲，逼出 ET 的读/写 drain 循环
    payload = b"x" * (2 * 1024 * 1024)
    expected = b"[lobby] alice: " + payload + b"\n"
    a.settimeout(15.0); b.settimeout(15.0)
    a.sendall(payload + b"\n")
    assert read_exact(b, expected) == expected  # 接收方收齐
    assert read_exact(a, expected) == expected  # 发送方自己的回显也收齐
    # 大消息之后小消息仍正常（连接没被写坏）
    b.sendall(b"hello\n")
    data = a.recv(4096)
    assert b"bob" in data and b"hello" in data, data
    print("et_mode_it OK")
finally:
    p.terminate()
    _, err = p.communicate(timeout=5)
# 服务器须打印 ET 模式标记（实现前该断言失败 —— TDD 红灯）
assert b"ET mode enabled" in err, err[:500]
print("et_mode_it marker OK")
