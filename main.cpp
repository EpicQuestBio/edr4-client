#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <random>
#include <cmath>
#include <iomanip>
#include <algorithm>
#include <memory>
#include <sstream>
#include <atomic>

#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

#include "er4commlib.h"
#include "er4commlib_errorcodes.h"

using namespace er4CommLib;

// ========================= Shared frame types =========================

struct SampleFrame {
    uint64_t tMs = 0;
    std::string source;       // "sim" or "e4"
    unsigned int sampleCount = 0;
    double minCurrent = 0.0;
    double maxCurrent = 0.0;
    double meanCurrent = 0.0;

    // Optional lightweight payload for graphing (decimated)
    std::vector<double> currentPreview;
};

static uint64_t nowMs() {
    using namespace std::chrono;
    return (uint64_t)duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count();
}

static std::string frameToJsonLine(const SampleFrame& f) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"type\":\"frame\",";
    oss << "\"source\":\"" << f.source << "\",";
    oss << "\"t_ms\":" << f.tMs << ",";
    oss << "\"samples\":" << f.sampleCount << ",";
    oss << "\"min_i\":" << std::fixed << std::setprecision(6) << f.minCurrent << ",";
    oss << "\"max_i\":" << std::fixed << std::setprecision(6) << f.maxCurrent << ",";
    oss << "\"mean_i\":" << std::fixed << std::setprecision(6) << f.meanCurrent << ",";
    oss << "\"preview\":[";
    for (size_t i = 0; i < f.currentPreview.size(); ++i) {
        if (i) oss << ",";
        oss << std::fixed << std::setprecision(6) << f.currentPreview[i];
    }
    oss << "]";
    oss << "}\n";
    return oss.str();
}

// ========================= TCP JSONL sender =========================

class TcpJsonlSink {
public:
    TcpJsonlSink(std::string host, uint16_t port)
        : host_(std::move(host)), port_(port) {
    }

    ~TcpJsonlSink() {
        close();
        cleanupWinsock();
    }

    bool init() {
        if (winsockInit_) return true;
        WSADATA wsaData{};
        int rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (rc != 0) {
            std::cerr << "WSAStartup failed: " << rc << "\n";
            return false;
        }
        winsockInit_ = true;
        return true;
    }

    bool connectIfNeeded() {
        if (sock_ != INVALID_SOCKET) return true;
        if (!init()) return false;

        struct addrinfo hints {};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        struct addrinfo* result = nullptr;
        std::string portStr = std::to_string(port_);
        int rc = getaddrinfo(host_.c_str(), portStr.c_str(), &hints, &result);
        if (rc != 0) {
            std::cerr << "getaddrinfo failed: " << rc << "\n";
            return false;
        }

        SOCKET s = INVALID_SOCKET;
        for (auto ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
            s = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
            if (s == INVALID_SOCKET) continue;

            if (::connect(s, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR) {
                closesocket(s);
                s = INVALID_SOCKET;
                continue;
            }
            break;
        }
        freeaddrinfo(result);

        if (s == INVALID_SOCKET) {
            // Not fatal: server may not be up yet.
            return false;
        }

        sock_ = s;
        std::cout << "Connected to Python server at " << host_ << ":" << port_ << "\n";
        return true;
    }

    bool sendLine(const std::string& line) {
        if (!connectIfNeeded()) return false;
        int total = 0;
        int len = (int)line.size();
        while (total < len) {
            int sent = send(sock_, line.data() + total, len - total, 0);
            if (sent == SOCKET_ERROR) {
                std::cerr << "Socket send failed. Disconnecting sink.\n";
                close();
                return false;
            }
            total += sent;
        }
        return true;
    }

    void close() {
        if (sock_ != INVALID_SOCKET) {
            closesocket(sock_);
            sock_ = INVALID_SOCKET;
        }
    }

private:
    void cleanupWinsock() {
        if (winsockInit_) {
            WSACleanup();
            winsockInit_ = false;
        }
    }

    std::string host_;
    uint16_t port_ = 0;
    SOCKET sock_ = INVALID_SOCKET;
    bool winsockInit_ = false;
};

// ========================= Data source abstraction =========================

class IDataSource {
public:
    virtual ~IDataSource() = default;
    virtual bool start() = 0;
    virtual bool readFrame(SampleFrame& outFrame) = 0; // blocking-ish / paced
    virtual void stop() = 0;
    virtual std::string name() const = 0;
};

// ========================= Simulation source =========================

class SimSource : public IDataSource {
public:
    SimSource(unsigned int samplesPerFrame = 64, int frameMs = 50)
        : samplesPerFrame_(samplesPerFrame), frameMs_(frameMs), rng_(std::random_device{}()) {
    }

    bool start() override {
        running_ = true;
        phase_ = 0.0;
        return true;
    }

    bool readFrame(SampleFrame& out) override {
        if (!running_) return false;

        std::normal_distribution<double> noise(0.0, 0.08);
        std::uniform_real_distribution<double> chance(0.0, 1.0);
        std::uniform_real_distribution<double> spikeAmp(0.3, 1.2);

        bool doSpike = (chance(rng_) < 0.08);
        int spikeStart = (int)(samplesPerFrame_ / 4);
        int spikeLen = 3 + (rng_() % 8);
        double spike = (rng_() % 2 == 0 ? 1.0 : -1.0) * spikeAmp(rng_);

        out = {};
        out.tMs = nowMs();
        out.source = "sim";
        out.sampleCount = samplesPerFrame_;
        out.minCurrent = 1e9;
        out.maxCurrent = -1e9;
        out.currentPreview.reserve(samplesPerFrame_);

        double sum = 0.0;
        for (unsigned int i = 0; i < samplesPerFrame_; ++i) {
            double x = std::sin(phase_) * 0.6 + std::sin(phase_ * 0.17) * 0.15 + noise(rng_);
            if (doSpike && i >= (unsigned int)spikeStart && i < (unsigned int)(spikeStart + spikeLen)) {
                x += spike;
            }
            phase_ += 0.09;

            out.minCurrent = std::min(out.minCurrent, x);
            out.maxCurrent = std::max(out.maxCurrent, x);
            sum += x;
            out.currentPreview.push_back(x);
        }
        out.meanCurrent = sum / (double)out.sampleCount;

        std::this_thread::sleep_for(std::chrono::milliseconds(frameMs_));
        return true;
    }

    void stop() override { running_ = false; }
    std::string name() const override { return "SimSource"; }

private:
    unsigned int samplesPerFrame_;
    int frameMs_;
    std::mt19937 rng_;
    double phase_ = 0.0;
    std::atomic<bool> running_{ false };
};

// ========================= E4 source =========================

class E4Source : public IDataSource {
public:
    explicit E4Source(int idleSleepMs = 10) : idleSleepMs_(idleSleepMs) {}

    bool start() override {
        // Detect + connect
        ErrorCodes_t detectError = detectDevices(deviceIds_);
        if (detectError != Success || deviceIds_.empty()) {
            return false;
        }

        std::cout << "Detected E4 device(s):\n";
        for (size_t i = 0; i < deviceIds_.size(); ++i) {
            std::cout << "  [" << i << "] " << deviceIds_[i] << "\n";
        }

        if (connect(deviceIds_) != Success) {
            std::cerr << "E4 connect failed.\n";
            return false;
        }

        if (getChannelsNumber(voltageChannelsNum_, currentChannelsNum_, gpChannelsNum_) != Success) {
            std::cerr << "E4 getChannelsNumber failed.\n";
            disconnect();
            return false;
        }

        std::cout << "E4 connected. V=" << voltageChannelsNum_
            << " I=" << currentChannelsNum_
            << " GP=" << gpChannelsNum_ << "\n";

        if (currentChannelsNum_ == 0) {
            std::cerr << "No current channels on E4.\n";
            disconnect();
            return false;
        }

        bestEffortConfigure();
        purgeData(false);

        running_ = true;
        return true;
    }

    bool readFrame(SampleFrame& out) override {
        if (!running_) return false;

        QueueStatus_t status{};
        ErrorCodes_t qs = getQueueStatus(status);
        if (qs != Success) {
            std::this_thread::sleep_for(std::chrono::milliseconds(idleSleepMs_));
            return false;
        }

        if (status.availableDataPackets == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(idleSleepMs_));
            return false;
        }

        unsigned int dataRead = 0;
        uint16_t* buffer = nullptr;
        ErrorCodes_t rd = readData(status.availableDataPackets, dataRead, buffer);
        if (rd != Success || dataRead == 0 || buffer == nullptr) {
            std::this_thread::sleep_for(std::chrono::milliseconds(idleSleepMs_));
            return false;
        }

        const unsigned int stride = voltageChannelsNum_ + currentChannelsNum_;
        if (stride == 0) return false;

        out = {};
        out.tMs = nowMs();
        out.source = "e4";
        out.sampleCount = dataRead;
        out.minCurrent = 1e9;
        out.maxCurrent = -1e9;

        // Keep a decimated preview to avoid huge JSON payloads
        unsigned int previewTarget = 64;
        unsigned int step = std::max(1u, dataRead / previewTarget);

        double sum = 0.0;
        unsigned int validCount = 0;

        for (unsigned int p = 0; p < dataRead; ++p) {
            unsigned int base = p * stride;
            unsigned int currIdx = base + voltageChannelsNum_; // current channel 0
            double currentVal = 0.0;
            ErrorCodes_t cv = convertCurrentValue(buffer[currIdx], 0, currentVal);
            if (cv != Success) continue;

            out.minCurrent = std::min(out.minCurrent, currentVal);
            out.maxCurrent = std::max(out.maxCurrent, currentVal);
            sum += currentVal;
            ++validCount;

            if (p % step == 0) {
                out.currentPreview.push_back(currentVal);
            }
        }

        if (validCount == 0) return false;
        out.meanCurrent = sum / (double)validCount;
        return true;
    }

    void stop() override {
        if (running_) {
            running_ = false;
            disconnect();
        }
    }

    std::string name() const override { return "E4Source"; }

    ~E4Source() override { stop(); }

private:
    void bestEffortConfigure() {
        // Current range
        std::vector<RangedMeasurement_t> currentRanges;
        std::vector<uint16_t> currentRangeDefaults;
        if (getCurrentRanges(currentRanges, currentRangeDefaults) == Success && !currentRanges.empty()) {
            unsigned int idx = (currentRanges.size() > 1) ? 1 : 0;
            setCurrentRange(idx);
        }

        // Sampling rate
        std::vector<Measurement_t> samplingRates;
        uint16_t defaultSampling = 0;
        if (getSamplingRates(samplingRates, defaultSampling) == Success && !samplingRates.empty()) {
            setSamplingRate(0); // vendor sample uses 0 == 1.25kHz
        }

        // Short offset comp
        if (currentChannelsNum_ > 0) {
            digitalOffsetCompensation(currentChannelsNum_, true);
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            digitalOffsetCompensation(currentChannelsNum_, false);
        }

        // Try starting a protocol so data flows
        std::vector<std::string> names, images;
        std::vector<std::vector<uint16_t>> voltages, times, slopes, frequencies, adims;
        if (getProtocolList(names, images, voltages, times, slopes, frequencies, adims) == Success &&
            !names.empty()) {
            int protocolId = (names.size() > 3) ? 3 : 0;
            if (selectVoltageProtocol(protocolId) == Success) {
                applyVoltageProtocol();
            }
        }
    }

    int idleSleepMs_;
    std::atomic<bool> running_{ false };

    std::vector<std::string> deviceIds_;
    uint32_t voltageChannelsNum_ = 0;
    uint32_t currentChannelsNum_ = 0;
    uint32_t gpChannelsNum_ = 0;
};

// ========================= Main =========================

struct Args {
    bool forceSim = false;
    int seconds = 0; // 0 => run forever
    std::string host = "127.0.0.1";
    uint16_t port = 8765;
};

static Args parseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        if (s == "--simulate") {
            a.forceSim = true;
        }
        else if (s.rfind("--seconds=", 0) == 0) {
            a.seconds = std::max(0, std::stoi(s.substr(10)));
        }
        else if (s.rfind("--host=", 0) == 0) {
            a.host = s.substr(7);
        }
        else if (s.rfind("--port=", 0) == 0) {
            int p = std::stoi(s.substr(7));
            a.port = (uint16_t)std::clamp(p, 1, 65535);
        }
    }
    return a;
}

static void printBanner(const Args& args) {
    std::cout << "mq_e4_client v2 (prototype)\n";
    std::cout << "  server: " << args.host << ":" << args.port << "\n";
    std::cout << "  mode:   " << (args.forceSim ? "SIM ONLY" : "AUTO (E4 -> SIM fallback)") << "\n";
    std::cout << "  run:    " << (args.seconds > 0 ? std::to_string(args.seconds) + "s" : "forever") << "\n";
}

int main(int argc, char** argv) {
    Args args = parseArgs(argc, argv);
    printBanner(args);

    std::unique_ptr<IDataSource> source;
    if (!args.forceSim) {
        auto e4 = std::make_unique<E4Source>();
        if (e4->start()) {
            source = std::move(e4);
        }
        else {
            std::cout << "No E4 detected / init failed. Falling back to simulation.\n";
        }
    }
    if (!source) {
        auto sim = std::make_unique<SimSource>();
        if (!sim->start()) {
            std::cerr << "Simulation source failed to start.\n";
            return 1;
        }
        source = std::move(sim);
    }

    TcpJsonlSink sink(args.host, args.port);

    auto start = std::chrono::steady_clock::now();
    uint64_t sentCount = 0;
    uint64_t localOnlyCount = 0;

    try {
        while (true) {
            if (args.seconds > 0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - start).count();
                if (elapsed >= args.seconds) break;
            }

            SampleFrame frame;
            if (!source->readFrame(frame)) {
                continue; // E4 path may occasionally have no data yet
            }

            std::string line = frameToJsonLine(frame);
            bool sent = sink.sendLine(line);

            if (sent) {
                ++sentCount;
            }
            else {
                ++localOnlyCount;
                // Fallback to stdout so you can still see activity if server isn't running
                std::cout << line;
            }

            // Optional periodic status
            if ((sentCount + localOnlyCount) % 100 == 0) {
                std::cerr << "[status] source=" << source->name()
                    << " sent=" << sentCount
                    << " local_only=" << localOnlyCount << "\n";
            }
        }
    }
    catch (const std::exception& ex) {
        std::cerr << "Fatal exception: " << ex.what() << "\n";
    }

    source->stop();
    sink.close();

    std::cout << "Done. sent=" << sentCount << " local_only=" << localOnlyCount << "\n";
    return 0;
}