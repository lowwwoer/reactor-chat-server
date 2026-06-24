#!/usr/bin/env bash
# 在 macOS 上运行：构建镜像并进入容器（项目目录挂到容器 /work）。
set -e
docker build -t netlab docker
docker run --rm -it -v "$(pwd)":/work -p 9000:9000 netlab bash
