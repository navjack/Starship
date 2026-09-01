#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace Starship {

class DiscordIpcClient {
  public:
    DiscordIpcClient() = default;
    ~DiscordIpcClient();

    DiscordIpcClient(const DiscordIpcClient&) = delete;
    DiscordIpcClient& operator=(const DiscordIpcClient&) = delete;

    bool Connect(const char* clientId);
    void Disconnect();
    bool IsConnected() const;
    bool SetActivity(const std::string& activityJson);
    void Pump();

  private:
    bool WriteFrame(uint32_t opCode, const std::string& payload);
    bool TryReadFrame(uint32_t& opCode, std::string& payload);
    void MarkFailed();

    int mSocket = -1;
    uint64_t mNonce = 1;
    std::vector<uint8_t> mReadBuffer;
};

} // namespace Starship
