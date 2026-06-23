// src/rpc/rpc_codec.h
#pragma once
#include <string>
#include <cstdint>
#include <cstring>
#include <arpa/inet.h>
#include "Buffer.h"

// 长度前缀编解码工具
class RpcCodec {
public:
    // 打包：长度(4字节网络序) + 数据
    static std::string pack(const std::string& data) {
        std::string result;
        int32_t len = static_cast<int32_t>(data.size());
        int32_t netLen = htonl(len);
        result.append(reinterpret_cast<const char*>(&netLen), sizeof netLen);
        result.append(data);
        return result;
    }

    // 带 id 打包：长度(4字节网络序) + id(8字节网络序) + 数据
    static std::string packWithId(uint64_t id, const std::string& data) {
        std::string result;
        int32_t len = static_cast<int32_t>(sizeof(uint64_t) + data.size());
        int32_t netLen = htonl(len);
        result.append(reinterpret_cast<const char*>(&netLen), sizeof netLen);
        uint64_t netId = htonll(id);
        result.append(reinterpret_cast<const char*>(&netId), sizeof netId);
        result.append(data);
        return result;
    }

    // 尝试从 Buffer 中读取一条完整消息，成功返回消息内容，失败返回空字符串
    static std::string tryUnpack(Buffer* buf) {
        if (buf->readableBytes() < 4) return "";
        int32_t netLen = 0;
        memcpy(&netLen, buf->peek(), 4);
        int32_t len = ntohl(netLen);
        if (len < 0 || len > 64 * 1024 * 1024) {
            buf->retrieveAll();
            return "";
        }
        if (buf->readableBytes() < 4 + static_cast<size_t>(len)) return "";
        buf->retrieve(4);
        return buf->retrieveAsString(len);
    }

    // 带 id 解包：长度(4字节) + id(8字节) + 数据，返回数据部分，id 通过引用传出
    static std::string tryUnpackWithId(Buffer* buf, uint64_t& id) {
        if (buf->readableBytes() < 4) return "";
        int32_t netLen = 0;
        memcpy(&netLen, buf->peek(), 4);
        int32_t len = ntohl(netLen);
        if (len < 0 || len > 64 * 1024 * 1024) {
            buf->retrieveAll();
            id = 0;
            return "";
        }
        if (buf->readableBytes() < 4 + static_cast<size_t>(len)) return "";
        buf->retrieve(4);
        if (len < static_cast<int32_t>(sizeof(uint64_t))) {
            buf->retrieveAll();
            id = 0;
            return "";
        }
        uint64_t netId = 0;
        memcpy(&netId, buf->peek(), sizeof(uint64_t));
        id = ntohll(netId);
        buf->retrieve(sizeof(uint64_t));
        return buf->retrieveAsString(len - sizeof(uint64_t));
    }

private:
    // 主机序 <-> 网络序 64位转换
    static uint64_t htonll(uint64_t val) {
        if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) {
            return ((uint64_t)htonl(val & 0xFFFFFFFF) << 32) | htonl(val >> 32);
        }
        return val;
    }
    static uint64_t ntohll(uint64_t val) {
        return htonll(val);
    }
};
