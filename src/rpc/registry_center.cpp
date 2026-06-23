#include "registry_center.h"

RegistryCenter& RegistryCenter::instance() {
    static RegistryCenter center;
    return center;
}

void RegistryCenter::registerService(const std::string& service, const std::string& address) {
    std::lock_guard<std::mutex> lock(mutex_);
    services_[service].insert(address);
}

void RegistryCenter::unregisterService(const std::string& service, const std::string& address) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = services_.find(service);
    if (it != services_.end()) {
        it->second.erase(address);
        if (it->second.empty()) {
            services_.erase(it);
        }
    }
}

std::set<std::string> RegistryCenter::discover(const std::string& service) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = services_.find(service);
    if (it != services_.end()) {
        return it->second;
    }
    return {};
}
