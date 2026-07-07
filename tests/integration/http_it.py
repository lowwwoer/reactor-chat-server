import http.client, os, subprocess, time
# HTTP demo 集成测试（附录 A 拉伸项）：复用 net/ 网络库写的最小 HTTP/1.1 服务器。
# 验证：GET 返回 200、Content-Type 为 text/html、正文回显解析出的方法与路径
#（证明请求行被正确解析），且 Content-Length 与正文一致（http.client 按长度精确读，不一致会挂）。
PORT = int(os.environ.get("HTTP_PORT", "8080"))
env = dict(os.environ, HTTP_PORT=str(PORT))
p = subprocess.Popen([os.environ.get("HTTP_BIN", "./build/http_server")], env=env)
time.sleep(0.5)

def get(path):
    # 服务器每个请求后 Connection: close，故每次用新连接（也顺带验证连接可反复建立）。
    c = http.client.HTTPConnection("127.0.0.1", PORT, timeout=3)
    c.request("GET", path)
    r = c.getresponse()
    body = r.read()  # 按 Content-Length 精确读取；长度不符会阻塞到超时
    ctype = r.getheader("Content-Type", "")
    c.close()
    return r.status, ctype, body

try:
    status, ctype, body = get("/hello")
    assert status == 200, status
    assert "text/html" in ctype, ctype
    assert b"GET" in body and b"/hello" in body, body

    # 换一条路径再来一次，确认多连接、路径解析都稳定
    status2, _, body2 = get("/a/b/c")
    assert status2 == 200 and b"/a/b/c" in body2, (status2, body2)
    print("http_it OK")
finally:
    p.terminate()
