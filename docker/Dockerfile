FROM ubuntu:22.04
RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ cmake make gdb python3 net-tools \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /work
