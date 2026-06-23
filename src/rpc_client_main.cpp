#include "rpc/rpc_channel.h"
#include "rpc/registry_client.h"
#include "rpc/custom_controller.h"
#include "math_service.pb.h"
#include <iostream>
#include <memory>

int main() {
    auto regClient = std::make_shared<RegistryClient>("127.0.0.1", 8500);
    RpcChannel channel(regClient, "MathService", LoadBalanceType::ROUND_ROBIN);

    // 同步调用 Add
    {
        math::AddRequest req;
        req.set_a(10);
        req.set_b(20);
        math::AddResponse resp;
        CustomController ctl;
        ctl.setTimeoutMs(1000);
        ctl.setMaxRetries(1);

        const auto* serviceDesc = math::AddRequest::descriptor()->file()->FindServiceByName("MathService");
        const auto* methodDesc = serviceDesc->FindMethodByName("Add");
        channel.CallMethod(methodDesc, &ctl, &req, &resp, nullptr);

        if (!ctl.Failed()) {
            std::cout << "Sync Add result: " << resp.sum() << std::endl;
        } else {
            std::cerr << "RPC failed: " << ctl.ErrorText() << std::endl;
        }
    }

    channel.close();
    return 0;
}
