import os, socket, subprocess, time
# 空闲连接踢除集成测试（附录 A 拉伸项）：CHAT_IDLE_SECS=2 启动服务器，
# 每条连接超过该时长无输入即被服务器主动半关闭（timerfd 定时器 + 续约链）。
# 验证两件事：① 沉默的连接会在超时后收到 EOF；② 持续发消息的连接不会被误踢。
env = dict(os.environ, CHAT_IDLE_SECS="2")
p = subprocess.Popen([os.environ.get("CHAT_BIN", "./build/chat_server")],
                     env=env, stderr=subprocess.PIPE)
time.sleep(0.5)

def conn():
    s = socket.create_connection(("127.0.0.1", 9000)); s.settimeout(0.2); return s

def drain(s):
    """读干净当前可读数据；对端已半关闭返回 True(EOF)，否则超时返回 False。"""
    try:
        while True:
            if s.recv(65536) == b"": return True
    except socket.timeout:
        return False

try:
    alice, bob = conn(), conn()
    for s, n in ((alice, "alice"), (bob, "bob")):
        s.sendall(f"/nick {n}\n/join lobby\n".encode())
    time.sleep(0.3)
    drain(alice); drain(bob)  # 清掉欢迎/昵称/加入提示

    alice_eof = bob_eof = False
    deadline = time.time() + 8.0
    # bob 每轮发一次 ping 保活（间隔 < 2s 空闲阈值）；alice 全程沉默，应被踢。
    while time.time() < deadline and not alice_eof:
        bob.sendall(b"ping\n")
        if drain(bob):   bob_eof = True
        if drain(alice): alice_eof = True
        time.sleep(0.3)

    assert alice_eof, "空闲连接未在超时后被服务器断开"
    assert not bob_eof, "活跃连接被误踢"
    bob.settimeout(2.0)          # bob 仍在线：/who 应有响应
    bob.sendall(b"/who\n")
    resp = bob.recv(4096)
    assert b"bob" in resp, resp
    print("idle_timeout_it OK")
finally:
    p.terminate()
    _, err = p.communicate(timeout=5)
# 服务器须打印空闲超时已启用的标记（实现前该断言失败 —— TDD 红灯）
assert b"idle timeout" in err, err[:500]
print("idle_timeout_it marker OK")
