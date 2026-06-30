import socket, subprocess, time
p = subprocess.Popen(["./build/chat_server"]); time.sleep(0.5)
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
