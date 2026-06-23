// src/InetAddress.cpp
#include "InetAddress.h"
#include <cstring>
#include <sstream>


InetAddress::InetAddress(uint16_t port, std::string ip) {
	memset(&addr_, 0, sizeof addr_);
	addr_.sin_family = AF_INET;
	addr_.sin_port = htons(port);
	inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr);
}

std::string InetAddress::toIpPort() const {
	char buf[64] = {};
	inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof buf);
	std::ostringstream oss;
	oss << buf << ":" << ntohs(addr_.sin_port);
	return oss.str();
}
