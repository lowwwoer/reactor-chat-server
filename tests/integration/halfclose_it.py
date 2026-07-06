import os, socket, subprocess, time
# 半关闭验证：发一条大消息后立刻 /quit。服务器必须等输出缓冲 flush 完才
# shutdownWrite（见 TcpConnection::shutdownInLoop / handleWrite），
# 因此客户端应先收齐整条广播回显，然后才读到 EOF——一个字节都不能丢。
p = subprocess.Popen([os.environ.get("CHAT_BIN", "./build/chat_server")]); time.sleep(0.5)
try:
    s = socket.create_connection(("127.0.0.1", 9000)); s.settimeout(0.5)
    s.sendall(b"/nick tom\n/join lobby\n")
    time.sleep(0.3)
    try:
        while True: s.recv(4096)      # 清掉欢迎/昵称/加入提示
    except socket.timeout: pass
    s.settimeout(10.0)
    payload = b"x" * (4 * 1024 * 1024)          # 4MB 单行，远超内核发送缓冲
    s.sendall(payload + b"\n/quit\n")
    expected = b"[lobby] tom: " + payload + b"\n"
    got = bytearray()
    while True:                        # 读到 EOF（服务器半关闭写端）为止
        chunk = s.recv(65536)
        if not chunk: break
        got.extend(chunk)
    assert bytes(got) == expected, f"收到 {len(got)} 字节，期望 {len(expected)} 字节"
    assert p.poll() is None, "服务器进程不应退出"
    print("halfclose_it OK")
finally:
    p.terminate()
