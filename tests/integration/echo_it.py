#!/usr/bin/env python3
# 集成测试：起 echo 服务器，连上去发一行，断言原样收回。
# 用法： python3 tests/integration/echo_it.py
# 可用环境变量 ECHO_BIN 指定可执行路径（如 sanitizer 版本）。
import socket, subprocess, time, os, sys

BIN = os.environ.get("ECHO_BIN", "./build/echo_server")
p = subprocess.Popen([BIN])
time.sleep(0.5)
try:
    s = socket.create_connection(("127.0.0.1", 9000))
    s.settimeout(2.0)
    s.sendall(b"hello\n")
    got = s.recv(64)
    assert got == b"hello\n", f"unexpected: {got!r}"
    print("echo_it OK")
finally:
    p.terminate()
