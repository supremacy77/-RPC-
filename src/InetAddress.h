// src/InetAddress.h
#pragma once
#include  <arpa/inet.h>
#include <string>
#include <cstring>

class InetAddress {
public:
	InetAddress() {
		memset(&addr_, 0,sizeof(addr_));
		addr_.sin_family = AF_INET;
	}
	InetAddress(uint16_t port, std::string ip = "0.0.0.0");    
	explicit InetAddress(const sockaddr_in& addr) : addr_(addr){}
	const sockaddr_in& getSockAddr() const { return addr_; }
	std::string toIpPort() const;
private:
	sockaddr_in addr_;

};
