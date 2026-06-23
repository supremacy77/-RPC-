#pragma once
#include <google/protobuf/service.h>
#include <string>

class CustomController : public google::protobuf::RpcController {
public:
    void Reset() override { failed_ = false; errText_.clear(); }
    bool Failed() const override { return failed_; }
    std::string ErrorText() const override { return errText_; }
    void StartCancel() override {}
    void SetFailed(const std::string& reason) override { failed_ = true; errText_ = reason; }
    bool IsCanceled() const override { return false; }
    void NotifyOnCancel(google::protobuf::Closure* callback) override {}

    // 扩展配置方法
    void setTimeoutMs(int ms) { timeoutMs_ = ms; }
    int timeoutMs() const { return timeoutMs_; }

    void setMaxRetries(int n) { maxRetries_ = n; }
    int maxRetries() const { return maxRetries_; }

    void setIdempotent(bool v) { idempotent_ = v; }
    bool isIdempotent() const { return idempotent_; }

private:
    bool failed_ = false;
    std::string errText_;
    int timeoutMs_ = 3000;   // 默认 3 秒
    int maxRetries_ = 2;     // 默认重试 2 次
    bool idempotent_ = true; // 默认幂等
};
