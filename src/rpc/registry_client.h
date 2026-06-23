#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <memory>
#include <chrono>
#include "httplib.h"

// 缓存条目：存储地址列表 + 写入时间戳
struct CacheItem {
    std::vector<std::string> addrs;
    std::chrono::steady_clock::time_point timestamp;
};

class RegistryClient {
public:
    // 注册中心地址+端口构造
    RegistryClient(const std::string& registryAddr, uint16_t registryPort,
                   // 缓存过期时间默认30秒，网络重试默认2次
                   std::chrono::seconds cacheExpire = std::chrono::seconds(30),
                   int retryTimes = 2);

    // 对外接口：获取服务地址列表
    std::vector<std::string> getAddresses(const std::string& service);

private:
    // 从注册中心HTTP拉取节点，带重试机制
    std::vector<std::string> fetchFromRegistry(const std::string& service);
    // 读取磁盘缓存
    std::vector<std::string> loadFromDisk(const std::string& service);
    // 写入磁盘缓存（加文件锁防并发错乱）
    void saveToDisk(const std::string& service, const std::vector<std::string>& addrs);

    // 成员变量
    std::string registryAddr_;
    uint16_t registryPort_;
    std::mutex mutex_;
    std::map<std::string, CacheItem> cache_;   // 带时间戳的内存缓存
    std::unique_ptr<httplib::Client> httpClient_;

    std::chrono::seconds cacheExpire_; // 缓存过期时长
    int retryTimes_;                   // HTTP失败重试次数
    std::mutex fileMutex_;             // 全局文件操作锁，防止多线程读写同一个缓存文件冲突
};
