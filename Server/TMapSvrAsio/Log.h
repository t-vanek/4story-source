#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace spdlog { class logger; }

namespace tmapsvr {

// Per-channel file logger. Each instance owns one file, named after the
// channel and stamped with creation time. Replaces the legacy Win32 CLog
// (CString / fopen / hardcoded C:\logs path) and drops the CPacket
// dependency by taking raw bytes for packet dumps.
class Log {
public:
    explicit Log(std::string name);
    ~Log();

    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;

    // Open `<directory>/<YYYY-MM-DD_HH-MM-SS>_<name>.log` for appending.
    // Creates the directory if missing. Returns false on filesystem error.
    bool Open(const std::filesystem::path& directory);

    void WriteMessage(std::string_view message);

    void WritePacket(std::uint16_t id, std::span<const std::uint8_t> bytes);

private:
    std::string m_name;
    std::shared_ptr<spdlog::logger> m_logger;
};

} // namespace tmapsvr
