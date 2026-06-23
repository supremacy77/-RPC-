#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <numeric>
#include <getopt.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <netinet/tcp.h>
#include "rpc_codec.h"
#include "rpc_header.pb.h"
#include "math_service.pb.h"

struct Config {
    std::string host = "127.0.0.1";
    int port = 9999;
    int threads = 4;
    int connections = 10;
    int duration = 10;
    int targetQps = 0;
    std::string method = "add";
};

struct WorkerStats {
    std::vector<int64_t> latencies_us;
    uint64_t total_requests = 0;
    uint64_t total_errors = 0;
};

static std::atomic<bool> g_stop{false};

static uint64_t htonll(uint64_t val) {
    if (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) {
        return ((uint64_t)htonl(val & 0xFFFFFFFF) << 32) | htonl(val >> 32);
    }
    return val;
}
static uint64_t ntohll(uint64_t val) {
    return htonll(val);
}

static int createConnection(const std::string& host, int port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    int yes = 1;
    setsockopt(fd, SOL_TCP, TCP_NODELAY, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid address: %s\n", host.c_str());
        ::close(fd);
        return -1;
    }

    if (::connect(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        perror("connect");
        ::close(fd);
        return -1;
    }

    return fd;
}

static bool readFully(int fd, void* buf, size_t len) {
    size_t total = 0;
    char* ptr = static_cast<char*>(buf);
    while (total < len) {
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLIN;
        pfd.revents = 0;

        int ret = poll(&pfd, 1, 3000);
        if (ret <= 0) {
            return false;
        }
        if (pfd.revents & (POLLERR | POLLHUP)) {
            return false;
        }

        ssize_t n = ::recv(fd, ptr + total, len - total, 0);
        if (n <= 0) {
            return false;
        }
        total += n;
    }
    return true;
}

static bool sendAll(int fd, const char* data, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = ::send(fd, data + total, len - total, MSG_NOSIGNAL);
        if (n <= 0) {
            return false;
        }
        total += n;
    }
    return true;
}

static void workerFunc(const std::string& host, int port, int numConnections, int durationSec,
                       int targetQps, const std::string& method, int threadId, WorkerStats& stats) {
    std::vector<int> fds;
    for (int i = 0; i < numConnections; i++) {
        int fd = createConnection(host, port);
        if (fd < 0) {
            fprintf(stderr, "[Thread %d] Failed to connect connection %d\n", threadId, i);
            continue;
        }
        fds.push_back(fd);
    }

    if (fds.empty()) {
        fprintf(stderr, "[Thread %d] No connections established, exiting\n", threadId);
        return;
    }

    fprintf(stdout, "[Thread %d] Connected to %s:%d (%d connections)\n",
            threadId, host.c_str(), port, (int)fds.size());
    fflush(stdout);

    uint64_t requestId = threadId * 1000000ULL;
    int64_t intervalUs = 0;
    if (targetQps > 0) {
        intervalUs = 1000000LL / targetQps;
    }

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(durationSec);
    int connIdx = 0;

    while (!g_stop.load(std::memory_order_relaxed) &&
           std::chrono::steady_clock::now() < deadline) {

        int fd = fds[connIdx % fds.size()];
        connIdx++;

        auto reqStart = std::chrono::high_resolution_clock::now();

        rpc::RpcRequest req;
        req.set_service_name("math.MathService");
        req.set_method_name(method == "sub" ? "Sub" : "Add");

        if (method == "sub") {
            math::SubRequest subReq;
            subReq.set_a(rand() % 1000);
            subReq.set_b(rand() % 1000);
            req.set_args(subReq.SerializeAsString());
        } else {
            math::AddRequest addReq;
            addReq.set_a(rand() % 1000);
            addReq.set_b(rand() % 1000);
            req.set_args(addReq.SerializeAsString());
        }

        std::string payload = req.SerializeAsString();
        uint64_t id = requestId++;
        std::string frame = RpcCodec::packWithId(id, payload);

        if (!sendAll(fd, frame.data(), frame.size())) {
            stats.total_errors++;
            continue;
        }

        uint32_t netLen = 0;
        if (!readFully(fd, &netLen, 4)) {
            stats.total_errors++;
            continue;
        }

        uint32_t respLen = ntohl(netLen);
        if (respLen < sizeof(uint64_t) || respLen > 64 * 1024 * 1024) {
            stats.total_errors++;
            continue;
        }

        std::vector<char> respBuf(respLen);
        if (!readFully(fd, respBuf.data(), respLen)) {
            stats.total_errors++;
            continue;
        }

        uint64_t respId = 0;
        memcpy(&respId, respBuf.data(), sizeof(uint64_t));
        respId = ntohll(respId);

        std::string respPayload(respBuf.data() + sizeof(uint64_t), respLen - sizeof(uint64_t));

        rpc::RpcResponse resp;
        if (!resp.ParseFromString(respPayload)) {
            stats.total_errors++;
            continue;
        }

        auto reqEnd = std::chrono::high_resolution_clock::now();
        int64_t latencyUs = std::chrono::duration_cast<std::chrono::microseconds>(reqEnd - reqStart).count();
        stats.latencies_us.push_back(latencyUs);
        stats.total_requests++;

        if (intervalUs > 0) {
            auto next = reqStart + std::chrono::microseconds(intervalUs);
            auto now = std::chrono::high_resolution_clock::now();
            if (next > now) {
                std::this_thread::sleep_until(next);
            }
        }
    }

    for (int fd : fds) {
        ::close(fd);
    }
}

static void parseArgs(int argc, char* argv[], Config& cfg) {
    static struct option longOpts[] = {
        {"host",        required_argument, nullptr, 'H'},
        {"port",        required_argument, nullptr, 'p'},
        {"threads",     required_argument, nullptr, 't'},
        {"connections", required_argument, nullptr, 'c'},
        {"duration",    required_argument, nullptr, 'd'},
        {"qps",         required_argument, nullptr, 'q'},
        {"method",      required_argument, nullptr, 'm'},
        {"help",        no_argument,       nullptr, 'h'},
        {nullptr, 0, nullptr, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "H:p:t:c:d:q:m:h", longOpts, nullptr)) != -1) {
        switch (opt) {
            case 'H': cfg.host = optarg; break;
            case 'p': cfg.port = atoi(optarg); break;
            case 't': cfg.threads = atoi(optarg); break;
            case 'c': cfg.connections = atoi(optarg); break;
            case 'd': cfg.duration = atoi(optarg); break;
            case 'q': cfg.targetQps = atoi(optarg); break;
            case 'm': cfg.method = optarg; break;
            case 'h':
                printf("Usage: %s [options]\n"
                       "  --host, -H        Server host (default: 127.0.0.1)\n"
                       "  --port, -p        Server port (default: 9999)\n"
                       "  --threads, -t     Worker threads (default: 4)\n"
                       "  --connections, -c  Connections per thread (default: 10)\n"
                       "  --duration, -d    Duration in seconds (default: 10)\n"
                       "  --qps, -q         Target QPS, 0=unlimited (default: 0)\n"
                       "  --method, -m      Method: add or sub (default: add)\n",
                       argv[0]);
                exit(0);
            default:
                fprintf(stderr, "Unknown option. Use --help for usage.\n");
                exit(1);
        }
    }
}

static void printResults(const std::vector<WorkerStats>& allStats, double elapsed) {
    uint64_t totalReqs = 0;
    uint64_t totalErrors = 0;
    std::vector<int64_t> allLatencies;

    for (auto& s : allStats) {
        totalReqs += s.total_requests;
        totalErrors += s.total_errors;
        allLatencies.insert(allLatencies.end(), s.latencies_us.begin(), s.latencies_us.end());
    }

    std::sort(allLatencies.begin(), allLatencies.end());

    printf("\n===== Benchmark Results =====\n");
    printf("Duration:     %.2fs\n", elapsed);
    printf("Total Reqs:   %lu\n", totalReqs);
    printf("Total Errors: %lu\n", totalErrors);
    printf("QPS:          %.2f\n", totalReqs / elapsed);

    if (!allLatencies.empty()) {
        int64_t minLat = allLatencies.front();
        int64_t maxLat = allLatencies.back();
        double avgLat = std::accumulate(allLatencies.begin(), allLatencies.end(), 0.0) / allLatencies.size();
        int64_t p50 = allLatencies[allLatencies.size() * 50 / 100];
        int64_t p95 = allLatencies[allLatencies.size() * 95 / 100];
        int64_t p99 = allLatencies[allLatencies.size() * 99 / 100];

        printf("Latency (us): min=%ld avg=%ld max=%ld p50=%ld p95=%ld p99=%ld\n",
               minLat, (int64_t)avgLat, maxLat, p50, p95, p99);
    }
    printf("=============================\n");
}

int main(int argc, char* argv[]) {
    Config cfg;
    parseArgs(argc, argv, cfg);

    printf("RPC Benchmark: host=%s port=%d threads=%d connections=%d duration=%ds qps=%d method=%s\n",
           cfg.host.c_str(), cfg.port, cfg.threads, cfg.connections, cfg.duration,
           cfg.targetQps, cfg.method.c_str());
    fflush(stdout);

    std::vector<std::thread> threads;
    std::vector<WorkerStats> allStats(cfg.threads);

    auto startTime = std::chrono::steady_clock::now();

    for (int i = 0; i < cfg.threads; i++) {
        threads.emplace_back(workerFunc, std::ref(cfg.host), cfg.port,
                             cfg.connections, cfg.duration, cfg.targetQps,
                             std::ref(cfg.method), i, std::ref(allStats[i]));
    }

    for (auto& t : threads) {
        t.join();
    }

    auto endTime = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(endTime - startTime).count();

    printResults(allStats, elapsed);

    return 0;
}
