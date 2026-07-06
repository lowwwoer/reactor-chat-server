import os, socket, subprocess, time
# 缺陷回归：/join 空房间名必须被拒绝。
# 修复前 join("") 会把连接塞进 rooms_[""]，而 session->room 仍是空串，断开时
# leave() 按「room 为空 = 没进过房」跳过删除 → 该连接对象/fd 永久滞留房间表。
p = subprocess.Popen([os.environ.get("CHAT_BIN", "./build/chat_server")]); time.sleep(0.5)
try:
    s = socket.create_connection(("127.0.0.1", 9000)); s.settimeout(0.5)
    s.sendall(b"/nick tom\n")
    time.sleep(0.2)
    try:
        while True: s.recv(4096)      # 清掉欢迎/昵称提示
    except socket.timeout: pass
    s.settimeout(2.0)
    s.sendall(b"/join \n")            # 空房间名
    data = s.recv(4096)
    # 期望收到拒绝提示；修复前收到的是发到房间 "" 的加入广播 "[] 系统: tom 加入了房间"
    assert "房间名".encode() in data, data
    print("join_empty_it OK")
finally:
    p.terminate()
