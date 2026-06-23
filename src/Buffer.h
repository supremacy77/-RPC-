// src/Buffer.h
#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <cstddef>
#include <sys/uio.h> //readv

class Buffer {
	public:
		static const size_t kCheapPrepend = 8; //预留头部空间（未来加头部用）
		static const size_t kInitialSize = 1024; //初始缓冲区大小
		explicit Buffer(size_t initialSize = kInitialSize) : buffer_(kCheapPrepend + initialSize), 
		readIndex_(kCheapPrepend),
		writeIndex_(kCheapPrepend)
	{}
		
//可读字节数
		size_t readableBytes() const { return writeIndex_ - readIndex_; }
//可写字节数
		size_t writableBytes() const { return buffer_.size() - writeIndex_; }
//已读预备区的大小
		size_t prependableBytes() const { return readIndex_; }
//拿到可读区首地址（只读）
		const char* peek() const { return &buffer_[readIndex_]; }
//消费len 字节
		void retrieve(size_t len) {
			if(len < readableBytes()) {
				readIndex_ += len;
			} else {
				retrieveAll();
			}
		}

		void retrieveAll() {
			readIndex_ = kCheapPrepend;
			writeIndex_ = kCheapPrepend;
		}
//读出len 字节并返回string,同时消费
		std::string retrieveAsString(size_t len) {
			std::string result(peek(),len);
			retrieve(len);
			return result;
		}
//追加数据
		void append(const char* data, size_t len);
//从fd读取数据到Buffer
		ssize_t readFd(int fd, int* savedErrno);
	

	private:
		std::vector<char> buffer_;
		size_t readIndex_;
		size_t writeIndex_;

//获取起始写地址
		char* beginWrite() { return &buffer_[writeIndex_]; }
		const char* beginWrite() const { return &buffer_[writeIndex_]; }
//确保有len 字节可写空间
		void ensureWritableBytes(size_t len) {
			if(writableBytes() < len) {
				makeSpace(len);
			}
		}
//整理或扩容
		void makeSpace(size_t len);
};
