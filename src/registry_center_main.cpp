#include "rpc/registry_center.h"
#include "httplib.h"
#include <iostream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
int main() {
    RegistryCenter& center = RegistryCenter::instance();
    httplib::Server svr;

    svr.Post("/register", [&center](const httplib::Request& req, httplib::Response& res) {
    json j;
    try {
        j = json::parse(req.body);
    } catch (json::parse_error&) {
        res.status = 400;
        res.set_content(R"({"error":"invalid JSON"})", "application/json");
        return;
    }
    if (!j.contains("service") || !j.contains("address") ||
        !j["service"].is_string() || !j["address"].is_string()) {
        res.status = 400;
        res.set_content(R"({"error":"missing fields"})", "application/json");
        return;
    }
    std::string service = j["service"];
    std::string address = j["address"];
    if (service.empty() || address.empty()) {
        res.status = 400;
        res.set_content(R"({"error":"empty values"})", "application/json");
        return;
    }
    center.registerService(service, address);
    res.set_content(R"({"status":"ok"})", "application/json");
});
    svr.Get("/discover", [&center](const httplib::Request& req, httplib::Response& res) {
    auto service = req.get_param_value("service");
    if (service.empty()) {
        res.status = 400;
        res.set_content(R"({"error":"missing service param"})", "application/json");
        return;
    }
    auto addrs = center.discover(service);
    json arr = addrs;  // 直接转换 set<string> -> json 数组
    res.set_content(arr.dump(), "application/json");
});
    std::cout << "RegistryCenter listening on 0.0.0.0:8500" << std::endl;
    svr.listen("0.0.0.0", 8500);
}
