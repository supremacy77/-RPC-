#pragma once
#include "InetAddress.h"

class Socket {
public:
	explicit Socket(int fd) : sockfd_(fd) {}
	Socket();  // 创建非阻塞socket
	~Socket();

	void bind(const InetAddress& localAddr);
	void listen();
	int accept(InetAddress* peerAddr);
	int fd() const { return sockfd_; }

private:
	int sockfd_;
};
