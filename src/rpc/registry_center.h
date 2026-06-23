#pragma once
#include <string>
#include <set>
#include <map>
#include <mutex>

class RegistryCenter {
public:
    static RegistryCenter& instance();

    void registerService(const std::string& service, const std::string& address);
    void unregisterService(const std::string& service, const std::string& address);
    std::set<std::string> discover(const std::string& service) const;

private:
    RegistryCenter() = default;
    RegistryCenter(const RegistryCenter&) = delete;
    RegistryCenter& operator=(const RegistryCenter&) = delete;

    mutable std::mutex mutex_;
    std::map<std::string, std::set<std::string>> services_;
};
