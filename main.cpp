
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
#include <functional>
#include <cctype>

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
    std::string source;
    unsigned int sampleCount = 0;
    double minCurrent = 0.0;
    double maxCurrent = 0.0;
    double meanCurrent = 0.0;
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

static std::string escapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += c; break;
        }
    }
    return out;
}

// ========================= Tiny JSON helpers =========================

static bool findJsonString(const std::string& json, const std::string& key, std::string& out) {
    const std::string needle = "\"" + key + "\"";
    size_t p = json.find(needle);
    if (p == std::string::npos) return false;
    p = json.find(':', p + needle.size());
    if (p == std::string::npos) return false;
    p = json.find('"', p);
    if (p == std::string::npos) return false;
    ++p;

    std::string value;
    bool escape = false;
    for (; p < json.size(); ++p) {
        char c = json[p];
        if (escape) {
            value.push_back(c);
            escape = false;
        } else if (c == '\\') {
            escape = true;
        } else if (c == '"') {
            out = value;
            return true;
        } else {
            value.push_back(c);
        }
    }
    return false;
}

static bool findJsonInt(const std::string& json, const std::string& key, int& out) {
    const std::string needle = "\"" + key + "\"";
    size_t p = json.find(needle);
    if (p == std::string::npos) return false;
    p = json.find(':', p + needle.size());
    if (p == std::string::npos) return false;
    ++p;
    while (p < json.size() && std::isspace((unsigned char)json[p])) ++p;

    size_t start = p;
    if (p < json.size() && (json[p] == '-' || json[p] == '+')) ++p;
    while (p < json.size() && std::isdigit((unsigned char)json[p])) ++p;
    if (p == start || (p == start + 1 && (json[start] == '-' || json[start] == '+'))) return false;

    try {
        out = std::stoi(json.substr(start, p - start));
        return true;
    } catch (...) {
        return false;
    }
}

static std::string mapVoltagePresetToSettingString(const std::string& preset) {
    if (preset == "negative") return "-1";
    if (preset == "neutral") return "0";
    if (preset == "positive") return "1";
    return "";
}

// ========================= TCP JSONL client =========================

class TcpJsonlClient {
public:
    TcpJsonlClient(std::string host, uint16_t port)
        : host_(std::move(host)), port_(port) {}

    ~TcpJsonlClient() {
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

        struct addrinfo hints{};
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
            return false;
        }

        sock_ = s;
        recvBuffer_.clear();
        std::cout << "Connected to server at " << host_ << ":" << port_ << "\n";
        return true;
    }

    bool sendLine(const std::string& line) {
        if (!connectIfNeeded()) return false;
        int total = 0;
        int len = (int)line.size();
        while (total < len) {
            int sent = send(sock_, line.data() + total, len - total, 0);
            if (sent == SOCKET_ERROR) {
                std::cerr << "Socket send failed. Disconnecting client.\n";
                close();
                return false;
            }
            total += sent;
        }
        return true;
    }

    void pollIncomingLines(const std::function<void(const std::string&)>& handler) {
        if (sock_ == INVALID_SOCKET) return;

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock_, &readfds);
        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        int ready = select(0, &readfds, nullptr, nullptr, &tv);
        if (ready == SOCKET_ERROR) {
            std::cerr << "select() failed; disconnecting client.\n";
            close();
            return;
        }
        if (ready == 0 || !FD_ISSET(sock_, &readfds)) return;

        char buf[4096];
        int n = recv(sock_, buf, sizeof(buf), 0);
        if (n == 0) {
            std::cerr << "Server closed connection.\n";
            close();
            return;
        }
        if (n == SOCKET_ERROR) {
            std::cerr << "recv() failed; disconnecting client.\n";
            close();
            return;
        }

        recvBuffer_.append(buf, n);

        size_t pos = 0;
        while ((pos = recvBuffer_.find('\n')) != std::string::npos) {
            std::string line = recvBuffer_.substr(0, pos);
            recvBuffer_.erase(0, pos + 1);
            if (!line.empty()) handler(line);
        }
    }

    void close() {
        if (sock_ != INVALID_SOCKET) {
            closesocket(sock_);
            sock_ = INVALID_SOCKET;
        }
        recvBuffer_.clear();
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
    std::string recvBuffer_;
};

// ========================= Data source abstraction =========================

class IDataSource {
public:
    virtual ~IDataSource() = default;
    virtual bool start() = 0;
    virtual bool readFrame(SampleFrame& outFrame) = 0;
    virtual void stop() = 0;
    virtual std::string name() const = 0;

    virtual bool applyVoltageSetting(int setting, std::string& message) = 0;
    virtual bool triggerZapper(std::string& message) = 0;
};

// ========================= Simulation source =========================

class SimSource : public IDataSource {
public:
    SimSource(unsigned int samplesPerFrame = 64, int frameMs = 50)
        : samplesPerFrame_(samplesPerFrame), frameMs_(frameMs), rng_(std::random_device{}()) {}

    bool start() override {
        running_ = true;
        phase_ = 0.0;
        simulatedVoltageSetting_ = 0;
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

        // Use voltage setting to slightly bias the simulated waveform.
        double bias = 0.18 * (double)simulatedVoltageSetting_;

        out = {};
        out.tMs = nowMs();
        out.source = "sim";
        out.sampleCount = samplesPerFrame_;
        out.minCurrent = 1e9;
        out.maxCurrent = -1e9;
        out.currentPreview.reserve(samplesPerFrame_);

        double sum = 0.0;
        for (unsigned int i = 0; i < samplesPerFrame_; ++i) {
            double x = std::sin(phase_) * 0.6 + std::sin(phase_ * 0.17) * 0.15 + noise(rng_) + bias;
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

    bool applyVoltageSetting(int setting, std::string& message) override {
        simulatedVoltageSetting_ = setting;
        std::ostringstream oss;
        oss << "Simulated voltage setting applied: " << setting;
        message = oss.str();
        return true;
    }

    bool triggerZapper(std::string& message) override {
        message = "Simulated zap triggered";
        return true;
    }

private:
    unsigned int samplesPerFrame_;
    int frameMs_;
    std::mt19937 rng_;
    double phase_ = 0.0;
    int simulatedVoltageSetting_ = 0;
    std::atomic<bool> running_{false};
};

// ========================= E4 source =========================

class E4Source : public IDataSource {
public:
    explicit E4Source(int idleSleepMs = 10) : idleSleepMs_(idleSleepMs) {}

    bool start() override {
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

        unsigned int previewTarget = 64;
        unsigned int step = std::max(1u, dataRead / previewTarget);

        double sum = 0.0;
        unsigned int validCount = 0;

        for (unsigned int p = 0; p < dataRead; ++p) {
            unsigned int base = p * stride;
            unsigned int currIdx = base + voltageChannelsNum_;
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

    bool applyVoltageSetting(int setting, std::string& message) override {
        if (!running_) {
            message = "E4 not connected";
            return false;
        }

        const int milliVolts = (setting < 0) ? -500 : (setting > 0 ? 500 : 0);
        Measurement_t v{};
        v.value = (double)milliVolts;
        v.prefix = UnitPfxMilli;
        v.unit = "V";

        std::string validationMessage;
        ErrorCodes_t last = Success;

        for (uint32_t ch = 0; ch < currentChannelsNum_; ++ch) {
            validationMessage.clear();
            last = checkVoltageOffset(ch, v, validationMessage);
            if (last != Success) {
                std::ostringstream oss;
                oss << "Voltage setting rejected on channel " << ch
                    << " (setting=" << setting << ", " << milliVolts << " mV)"
                    << ". checkVoltageOffset returned " << (int)last;
                if (!validationMessage.empty()) oss << ": " << validationMessage;
                message = oss.str();
                return false;
            }

            last = setVoltageOffset(ch, v);
            if (last != Success) {
                std::ostringstream oss;
                oss << "setVoltageOffset failed on channel " << ch
                    << " with error " << (int)last;
                message = oss.str();
                return false;
            }
        }

        std::ostringstream oss;
        oss << "Applied E4 voltage setting " << setting
            << " => " << milliVolts << " mV on " << currentChannelsNum_ << " channel(s)";
        message = oss.str();
        return true;
    }

    bool triggerZapper(std::string& message) override {
        if (!running_) {
            message = "E4 not connected";
            return false;
        }

        bool zappable = false;
        bool singleChannelZap = false;
        ErrorCodes_t hz = hasZap(zappable, singleChannelZap);
        if (hz != Success) {
            std::ostringstream oss;
            oss << "hasZap failed with error " << (int)hz;
            message = oss.str();
            return false;
        }
        if (!zappable) {
            message = "Device reports zap feature unavailable";
            return false;
        }

        const uint16_t allChannelsIndex = (uint16_t)currentChannelsNum_;
        ErrorCodes_t ret = zap(allChannelsIndex);
        if (ret != Success) {
            std::ostringstream oss;
            oss << "zap(" << allChannelsIndex << ") failed with error " << (int)ret;
            message = oss.str();
            return false;
        }

        std::ostringstream oss;
        oss << "Triggered E4 zap on all channels"
            << " (singleChannelZap=" << (singleChannelZap ? "true" : "false") << ")";
        message = oss.str();
        return true;
    }

    ~E4Source() override { stop(); }

private:
    void bestEffortConfigure() {
        std::vector<RangedMeasurement_t> currentRanges;
        std::vector<uint16_t> currentRangeDefaults;
        if (getCurrentRanges(currentRanges, currentRangeDefaults) == Success && !currentRanges.empty()) {
            unsigned int idx = (currentRanges.size() > 1) ? 1 : 0;
            setCurrentRange(idx);
        }

        std::vector<Measurement_t> samplingRates;
        uint16_t defaultSampling = 0;
        if (getSamplingRates(samplingRates, defaultSampling) == Success && !samplingRates.empty()) {
            setSamplingRate(0);
        }

        if (currentChannelsNum_ > 0) {
            digitalOffsetCompensation(currentChannelsNum_, true);
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            digitalOffsetCompensation(currentChannelsNum_, false);
        }

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
    std::atomic<bool> running_{false};

    std::vector<std::string> deviceIds_;
    uint32_t voltageChannelsNum_ = 0;
    uint32_t currentChannelsNum_ = 0;
    uint32_t gpChannelsNum_ = 0;
};

// ========================= Command handling =========================

static void sendAck(TcpJsonlClient& client,
                    const std::string& action,
                    bool ok,
                    const std::string& message,
                    const std::string& rawLine) {
    std::ostringstream oss;
    oss << "{";
    oss << "\"type\":\"device_ack\",";
    oss << "\"action\":\"" << escapeJson(action) << "\",";
    oss << "\"ok\":" << (ok ? "true" : "false") << ",";
    oss << "\"message\":\"" << escapeJson(message) << "\",";
    oss << "\"raw\":\"" << escapeJson(rawLine) << "\"";
    oss << "}\n";
    client.sendLine(oss.str());
}

static void handleIncomingCommandLine(const std::string& line, IDataSource& source, TcpJsonlClient& client) {
    std::cout << "[recv] " << line << "\n";

    std::string type;
    findJsonString(line, "type", type);

    // Future-proof: support either browser-style control_intent or server-style device_command.
    if (type == "control_intent") {
        std::string intent;
        if (!findJsonString(line, "intent", intent)) {
            sendAck(client, "control_intent", false, "Missing intent field", line);
            return;
        }

        if (intent == "set_voltage_setting") {
            int value = 0;
            if (!findJsonInt(line, "value", value)) {
                sendAck(client, intent, false, "Missing integer params.value", line);
                return;
            }

            std::string msg;
            bool ok = source.applyVoltageSetting(value, msg);
            std::cout << "[apply] " << msg << "\n";
            sendAck(client, intent, ok, msg, line);
            return;
        }

        if (intent == "trigger_zapper") {
            std::string msg;
            bool ok = source.triggerZapper(msg);
            std::cout << "[apply] " << msg << "\n";
            sendAck(client, intent, ok, msg, line);
            return;
        }

        sendAck(client, intent, false, "Unsupported control intent", line);
        return;
    }

    if (type == "device_command") {
        std::string command;
        if (!findJsonString(line, "command", command)) {
            sendAck(client, "device_command", false, "Missing command field", line);
            return;
        }

        if (command == "apply_voltage_preset") {
            std::string preset;
            if (!findJsonString(line, "preset", preset)) {
                sendAck(client, command, false, "Missing args.preset", line);
                return;
            }

            std::string mapped = mapVoltagePresetToSettingString(preset);
            if (mapped.empty()) {
                sendAck(client, command, false, "Unsupported preset value", line);
                return;
            }

            int value = std::stoi(mapped);
            std::string msg;
            bool ok = source.applyVoltageSetting(value, msg);
            std::cout << "[apply] " << msg << "\n";
            sendAck(client, command, ok, msg, line);
            return;
        }

        if (command == "trigger_zap_preset") {
            std::string msg;
            bool ok = source.triggerZapper(msg);
            std::cout << "[apply] " << msg << "\n";
            sendAck(client, command, ok, msg, line);
            return;
        }

        sendAck(client, command, false, "Unsupported device command", line);
        return;
    }

    // Always log every command-like line, even if unsupported.
    sendAck(client, type.empty() ? "unknown" : type, false, "Ignored message type", line);
}

// ========================= Main =========================

struct Args {
    bool forceSim = false;
    int seconds = 0;
    std::string host = "127.0.0.1";
    uint16_t port = 8765;
};

static Args parseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        if (s == "--simulate") {
            a.forceSim = true;
        } else if (s.rfind("--seconds=", 0) == 0) {
            a.seconds = std::max(0, std::stoi(s.substr(10)));
        } else if (s.rfind("--host=", 0) == 0) {
            a.host = s.substr(7);
        } else if (s.rfind("--port=", 0) == 0) {
            int p = std::stoi(s.substr(7));
            if (p < 1) p = 1;
            if (p > 65535) p = 65535;
            a.port = (uint16_t)p;
        }
    }
    return a;
}

static void printBanner(const Args& args) {
    std::cout << "mq_e4_client v3 (stream + receive commands)\n";
    std::cout << "  server: " << args.host << ":" << args.port << "\n";
    std::cout << "  mode:   " << (args.forceSim ? "SIM ONLY" : "AUTO (E4 -> SIM fallback)") << "\n";
    std::cout << "  run:    " << (args.seconds > 0 ? std::to_string(args.seconds) + "s" : "forever") << "\n";
    std::cout << "  controls supported: set_voltage_setting, trigger_zapper\n";
}

int main(int argc, char** argv) {
    Args args = parseArgs(argc, argv);
    printBanner(args);

    std::unique_ptr<IDataSource> source;
    if (!args.forceSim) {
        auto e4 = std::make_unique<E4Source>();
        if (e4->start()) {
            source = std::move(e4);
        } else {
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

    TcpJsonlClient client(args.host, args.port);

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

            client.pollIncomingLines([&](const std::string& line) {
                handleIncomingCommandLine(line, *source, client);
            });

            SampleFrame frame;
            if (!source->readFrame(frame)) {
                continue;
            }

            std::string line = frameToJsonLine(frame);
            bool sent = client.sendLine(line);

            if (sent) {
                ++sentCount;
            } else {
                ++localOnlyCount;
                std::cout << line;
            }

            client.pollIncomingLines([&](const std::string& incoming) {
                handleIncomingCommandLine(incoming, *source, client);
            });

            if ((sentCount + localOnlyCount) % 100 == 0) {
                std::cerr << "[status] source=" << source->name()
                          << " sent=" << sentCount
                          << " local_only=" << localOnlyCount << "\n";
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "Fatal exception: " << ex.what() << "\n";
    }

    source->stop();
    client.close();

    std::cout << "Done. sent=" << sentCount << " local_only=" << localOnlyCount << "\n";
    return 0;
}
