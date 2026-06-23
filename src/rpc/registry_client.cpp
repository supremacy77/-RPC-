#include "registry_client.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>     
#include <nlohmann/json.hpp>
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <iostream>
#include <thread>        
using json = nlohmann::json;
using namespace std::chrono;
using json = nlohmann::json;
using namespace std::chrono;

RegistryClient::RegistryClient(const std::string& registryAddr, uint16_t registryPort,
                               seconds cacheExpire, int retryTimes)
    : registryAddr_(registryAddr), registryPort_(registryPort),
      cacheExpire_(cacheExpire), retryTimes_(retryTimes)
{
    httpClient_ = std::make_unique<httplib::Client>(registryAddr_.c_str(), registryPort_);
    // 连接超时1秒，单次请求总超时2秒
    httpClient_->set_connection_timeout(1, 0);
    httpClient_->set_read_timeout(2, 0);
}

std::vector<std::string> RegistryClient::getAddresses(const std::string& service)
{
    // 1. 查询内存缓存，判断是否过期
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(service);
        if (it != cache_.end())
        {
            auto now = steady_clock::now();
            auto diff = duration_cast<seconds>(now - it->second.timestamp);
            if (diff < cacheExpire_)
            {
                // 未过期，直接返回缓存
                return it->second.addrs;
            }
            // 已过期，标记失效，后续重新拉取
        }
    }

    // 2. 尝试请求注册中心（带重试）
    std::vector<std::string> addrs = fetchFromRegistry(service);
    if (!addrs.empty())
    {
        // 拉取成功：更新磁盘 + 更新内存缓存（带当前时间戳）
        saveToDisk(service, addrs);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            cache_[service].addrs = addrs;
            cache_[service].timestamp = steady_clock::now();
        }
        return addrs;
    }

    // 3. 网络全部失败，降级读取磁盘旧缓存兜底
    addrs = loadFromDisk(service);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_[service].addrs = addrs;
        cache_[service].timestamp = steady_clock::now();
    }
    return addrs;
}

std::vector<std::string> RegistryClient::fetchFromRegistry(const std::string& service)
{
    std::string path = "/discover?service=" + service;
    // 循环重试
    for (int i = 0; i <= retryTimes_; ++i)
    {
        auto res = httpClient_->Get(path.c_str());
        if (res && res->status == 200)
        {
            try
            {
                json arr = json::parse(res->body);
                if (!arr.is_array())
                    continue;
                return arr.get<std::vector<std::string>>();
            }
            catch (json::parse_error&)
            {
                // JSON解析失败，进入下一次重试
                continue;
            }
        }
        // 短暂休眠再重试
        std::this_thread::sleep_for(milliseconds(100));
    }
    // 多次重试全部失败
    return {};
}

std::vector<std::string> RegistryClient::loadFromDisk(const std::string& service)
{
    std::string filename = service + "_cache.txt";
    std::lock_guard<std::mutex> lock(fileMutex_);

    std::ifstream fin(filename);
    if (!fin.is_open())
        return {};

    std::vector<std::string> addrs;
    std::string line;
    while (std::getline(fin, line))
    {
        if (!line.empty())
            addrs.push_back(line);
    }
    fin.close();
    return addrs;
}

void RegistryClient::saveToDisk(const std::string& service, const std::vector<std::string>& addrs)
{
    std::string filename = service + "_cache.txt";
    std::lock_guard<std::mutex> lock(fileMutex_);

    // 方案：先写临时文件，写完重命名，避免中途断电文件损坏
    std::string tmpFile = filename + ".tmp";
    std::ofstream fout(tmpFile);
    if (!fout.is_open())
        return;

    for (const auto& addr : addrs)
    {
        fout << addr << '\n';
    }
    fout.flush();
    fout.close();

    // 原子替换原文件
    rename(tmpFile.c_str(), filename.c_str());
}
