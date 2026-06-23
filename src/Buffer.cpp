// src/Buffer.cpp
#include "Buffer.h"
#include <errno.h>
#include <unistd.h>


void Buffer::append(const char* data, size_t len) {
	ensureWritableBytes(len);
	std::copy(data, data + len, beginWrite());
	writeIndex_ += len;
}

void Buffer::makeSpace(size_t len) {
	if(writableBytes() + prependableBytes() < len + kCheapPrepend) {
		buffer_.resize(writeIndex_ + len);
	} else {
		size_t readable = readableBytes();
		std::copy(peek(),peek() + readable, &buffer_[kCheapPrepend]);
		readIndex_ = kCheapPrepend;
		writeIndex_ = readIndex_ + readable;
	}
}


ssize_t Buffer::readFd(int fd, int* savedErrno) {
	char extrabuf[65536];//64KB 栈空间
	struct iovec vec[2];
	const size_t writable = writableBytes();
	vec[0].iov_base = beginWrite();
	vec[0].iov_len = writable;
	vec[1].iov_base = extrabuf;
	vec[1].iov_len = sizeof extrabuf;

	const ssize_t n = ::readv(fd, vec, 2);
	if(n <0) {
		*savedErrno = errno;
	} else if(static_cast<size_t>(n) <= writable) {
		writeIndex_ += n;
	} else {
		writeIndex_ = buffer_.size();
		append(extrabuf, n - writable);
	}
	return n;
}
