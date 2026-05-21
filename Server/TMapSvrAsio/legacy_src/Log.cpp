#include "Log.h"

#include <chrono>
#include <ctime>
#include <iterator>
#include <utility>

#include <fmt/format.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace tmapsvr {

namespace {

std::string TimestampForFilename()
{
    using clock = std::chrono::system_clock;
    const auto now = clock::to_time_t(clock::now());
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &now);
#else
    localtime_r(&now, &tm);
#endif
    return fmt::format("{:04}-{:02}-{:02}_{:02}-{:02}-{:02}",
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec);
}

} // namespace

Log::Log(std::string name)
    : m_name(name.empty() ? std::string{"Unnamed"} : std::move(name))
{
}

Log::~Log() = default;

bool Log::Open(const std::filesystem::path& directory)
{
    try {
        std::filesystem::create_directories(directory);
        const auto path = directory / fmt::format("{}_{}.log", TimestampForFilename(), m_name);
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), /*truncate*/ false);
        m_logger = std::make_shared<spdlog::logger>(m_name, std::move(sink));
        m_logger->set_pattern("%v");
        m_logger->flush_on(spdlog::level::info);
        return true;
    } catch (const spdlog::spdlog_ex&) {
        m_logger.reset();
        return false;
    }
}

void Log::WriteMessage(std::string_view message)
{
    if (m_logger) {
        m_logger->info("{}", message);
    }
}

void Log::WritePacket(std::uint16_t id, std::span<const std::uint8_t> bytes)
{
    if (!m_logger) return;

    m_logger->info("PACKET {:x} SIZE={}", id, bytes.size());

    std::string hex;
    hex.reserve(bytes.size() * 2 + bytes.size() / 128);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        fmt::format_to(std::back_inserter(hex), "{:02X}", bytes[i]);
        if (i != 0 && i % 128 == 0) {
            hex.push_back('\n');
        }
    }
    m_logger->info("{}", hex);
}

} // namespace tmapsvr
