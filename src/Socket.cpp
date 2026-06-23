#include "Socket.h"
#include <sys/socket.h>
#include <unistd.h>   // close
#include <cstring>
#include <iostream>

Socket::Socket() {
	sockfd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if(sockfd_ < 0) {
		perror("socket");
		exit(1);
	}
}

Socket::~Socket() {
	if (sockfd_ >= 0) {
		::close(sockfd_);
	}
}

void Socket::bind(const InetAddress& localAddr) {
	const sockaddr_in& addr = localAddr.getSockAddr();
	if(::bind(sockfd_, (sockaddr*)&addr, sizeof addr) < 0) {
		perror("bind");
		exit(1);
	}
}

void Socket::listen() {
	if(::listen(sockfd_, SOMAXCONN) < 0) {
		perror("listen");
		exit(1);
	}
}

int Socket::accept(InetAddress* peerAddr) {
	sockaddr_in addr;
	socklen_t len = sizeof addr;
	memset(&addr, 0, sizeof addr);
	int connfd = ::accept4(sockfd_, (sockaddr*)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
	if(connfd >= 0) {
		*peerAddr = InetAddress(addr);
	}
	return connfd;
}
