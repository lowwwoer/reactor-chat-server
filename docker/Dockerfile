# 国内网络环境：基础镜像走 DaoCloud 镜像源（网络无碍时可改回 ubuntu:22.04）
FROM docker.m.daocloud.io/library/ubuntu:22.04

# 把 apt 源换成国内镜像（USTC），避免 ports/archive.ubuntu.com 拉包超时。
# 主机名整体替换，路径保留：arm64 用 ubuntu-ports、amd64 用 ubuntu，均正确。
RUN sed -i -e 's@//ports.ubuntu.com@//mirrors.ustc.edu.cn@g' \
           -e 's@//archive.ubuntu.com@//mirrors.ustc.edu.cn@g' \
           -e 's@//security.ubuntu.com@//mirrors.ustc.edu.cn@g' /etc/apt/sources.list \
 && apt-get update && apt-get install -y --no-install-recommends \
    g++ cmake make gdb python3 net-tools \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /work
