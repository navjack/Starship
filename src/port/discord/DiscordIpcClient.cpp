#include "DiscordIpcClient.h"

#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <spdlog/spdlog.h>

namespace Starship {
namespace {

constexpr uint32_t DISCORD_OP_HANDSHAKE = 0;
constexpr uint32_t DISCORD_OP_FRAME = 1;
constexpr uint32_t DISCORD_OP_CLOSE = 2;
constexpr uint32_t DISCORD_OP_PING = 3;
constexpr uint32_t DISCORD_OP_PONG = 4;

bool WriteAll(int fd, const void* data, size_t size) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    while (size > 0) {
        const ssize_t written = write(fd, bytes, size);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (written == 0) {
            return false;
        }
        bytes += written;
        size -= static_cast<size_t>(written);
    }
    return true;
}

std::string JsonEscape(const char* text) {
    std::string escaped;
    for (const char* at = text; *at != '\0'; ++at) {
        switch (*at) {
            case '\\':
                escaped += "\\\\";
                break;
            case '"':
                escaped += "\\\"";
                break;
            case '\b':
                escaped += "\\b";
                break;
            case '\f':
                escaped += "\\f";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped += *at;
                break;
        }
    }
    return escaped;
}

void AddSocketDir(std::vector<std::string>& dirs, const char* dir) {
    if ((dir == nullptr) || (dir[0] == '\0')) {
        return;
    }

    std::string normalized = dir;
    while ((normalized.size() > 1) && (normalized.back() == '/')) {
        normalized.pop_back();
    }

    for (const std::string& existing : dirs) {
        if (existing == normalized) {
            return;
        }
    }
    dirs.push_back(std::move(normalized));
}

std::vector<std::string> BuildSocketDirs() {
    std::vector<std::string> dirs;
    AddSocketDir(dirs, getenv("XDG_RUNTIME_DIR"));
    AddSocketDir(dirs, getenv("TMPDIR"));
    AddSocketDir(dirs, getenv("TMP"));
    AddSocketDir(dirs, getenv("TEMP"));
    AddSocketDir(dirs, "/tmp");
    return dirs;
}

} // namespace

DiscordIpcClient::~DiscordIpcClient() {
    Disconnect();
}

bool DiscordIpcClient::Connect(const char* clientId) {
    Disconnect();

    for (const std::string& dir : BuildSocketDirs()) {
        for (int index = 0; index < 10; index++) {
            const int fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (fd < 0) {
                return false;
            }

#ifdef SO_NOSIGPIPE
            int noSigpipe = 1;
            setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &noSigpipe, sizeof(noSigpipe));
#endif

            sockaddr_un addr = {};
            addr.sun_family = AF_UNIX;
            snprintf(addr.sun_path, sizeof(addr.sun_path), "%s/discord-ipc-%d", dir.c_str(), index);

            if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
                close(fd);
                continue;
            }

            const int flags = fcntl(fd, F_GETFL, 0);
            if (flags >= 0) {
                fcntl(fd, F_SETFL, flags | O_NONBLOCK);
            }

            mSocket = fd;
            const std::string handshake = "{\"v\":1,\"client_id\":\"" + JsonEscape(clientId) + "\"}";
            if (!WriteFrame(DISCORD_OP_HANDSHAKE, handshake)) {
                Disconnect();
                continue;
            }

            SPDLOG_INFO("Discord Rich Presence connected");
            return true;
        }
    }

    return false;
}

void DiscordIpcClient::Disconnect() {
    if (mSocket >= 0) {
        close(mSocket);
        mSocket = -1;
    }
    mReadBuffer.clear();
}

bool DiscordIpcClient::IsConnected() const {
    return mSocket >= 0;
}

bool DiscordIpcClient::SetActivity(const std::string& activityJson) {
    if (!IsConnected()) {
        return false;
    }

    const std::string nonce = std::to_string(mNonce++);
    const std::string payload = "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":" + std::to_string(getpid()) +
                                ",\"activity\":" + activityJson + "},\"nonce\":\"" + nonce + "\"}";

    if (!WriteFrame(DISCORD_OP_FRAME, payload)) {
        MarkFailed();
        return false;
    }

    Pump();
    return true;
}

void DiscordIpcClient::Pump() {
    if (!IsConnected()) {
        return;
    }

    std::array<char, 4096> buffer;
    for (;;) {
        const ssize_t bytesRead = read(mSocket, buffer.data(), buffer.size());
        if (bytesRead > 0) {
            const auto* bytes = reinterpret_cast<const uint8_t*>(buffer.data());
            mReadBuffer.insert(mReadBuffer.end(), bytes, bytes + bytesRead);
            continue;
        }
        if (bytesRead == 0) {
            MarkFailed();
            return;
        }
        if ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR)) {
            return;
        }
        MarkFailed();
        return;
    }

    for (;;) {
        uint32_t opCode = 0;
        std::string payload;
        if (!TryReadFrame(opCode, payload)) {
            return;
        }

        switch (opCode) {
            case DISCORD_OP_FRAME:
                if (payload.find("\"evt\":\"ERROR\"") != std::string::npos) {
                    SPDLOG_WARN("Discord Rich Presence error: {}", payload);
                } else {
                    SPDLOG_DEBUG("Discord Rich Presence frame: {}", payload);
                }
                break;
            case DISCORD_OP_CLOSE:
                SPDLOG_WARN("Discord Rich Presence closed by Discord: {}", payload);
                MarkFailed();
                return;
            case DISCORD_OP_PING:
                WriteFrame(DISCORD_OP_PONG, payload);
                break;
            case DISCORD_OP_PONG:
                break;
            default:
                SPDLOG_DEBUG("Discord Rich Presence ignored op {}: {}", opCode, payload);
                break;
        }
    }
}

bool DiscordIpcClient::WriteFrame(uint32_t opCode, const std::string& payload) {
    const uint32_t payloadSize = static_cast<uint32_t>(payload.size());

    return WriteAll(mSocket, &opCode, sizeof(opCode)) && WriteAll(mSocket, &payloadSize, sizeof(payloadSize)) &&
           WriteAll(mSocket, payload.data(), payload.size());
}

bool DiscordIpcClient::TryReadFrame(uint32_t& opCode, std::string& payload) {
    constexpr size_t HEADER_SIZE = sizeof(uint32_t) * 2;
    if (mReadBuffer.size() < HEADER_SIZE) {
        return false;
    }

    uint32_t payloadSize = 0;
    memcpy(&opCode, mReadBuffer.data(), sizeof(opCode));
    memcpy(&payloadSize, mReadBuffer.data() + sizeof(opCode), sizeof(payloadSize));

    if (payloadSize > (1024 * 1024)) {
        SPDLOG_WARN("Discord Rich Presence received oversized frame: {} bytes", payloadSize);
        MarkFailed();
        return false;
    }

    const size_t frameSize = HEADER_SIZE + payloadSize;
    if (mReadBuffer.size() < frameSize) {
        return false;
    }

    payload.assign(reinterpret_cast<const char*>(mReadBuffer.data() + HEADER_SIZE), payloadSize);
    mReadBuffer.erase(mReadBuffer.begin(), mReadBuffer.begin() + static_cast<std::ptrdiff_t>(frameSize));
    return true;
}

void DiscordIpcClient::MarkFailed() {
    SPDLOG_INFO("Discord Rich Presence disconnected");
    Disconnect();
}

} // namespace Starship
