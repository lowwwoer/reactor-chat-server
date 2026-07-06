import os, socket, subprocess, time
# CHAT_BIN 指定被测可执行，便于切换 sanitizer 构建：
#   CHAT_BIN=./build-tsan/chat_server python3 tests/integration/chat_it.py
p = subprocess.Popen([os.environ.get("CHAT_BIN", "./build/chat_server")]); time.sleep(0.5)
def conn():
    s = socket.create_connection(("127.0.0.1", 9000)); s.settimeout(2.0); return s
try:
    a, b = conn(), conn()
    for s, n in ((a,"alice"), (b,"bob")):
        s.sendall(f"/nick {n}\n".encode()); s.sendall(b"/join lobby\n")
    time.sleep(0.3)
    # 清掉欢迎/加入提示
    for s in (a, b):
        try:
            while True: s.recv(4096)
        except socket.timeout: pass
    a.settimeout(2.0); b.settimeout(2.0)
    a.sendall(b"hello\n")
    data = b.recv(4096)
    assert b"alice" in data and b"hello" in data, data
    print("chat_it OK")
finally:
    p.terminate()
