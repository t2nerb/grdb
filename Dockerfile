FROM ubuntu:latest

WORKDIR /root/

RUN \
		apt update && \
		apt install -y tmux gcc make

RUN \
		rm -rf /var/lib/apt/lists/*
