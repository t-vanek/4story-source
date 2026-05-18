#pragma once

// LogSink — abstracts the destination of a parsed _LOG_DATA_ record.
//   * SociLogSink — INSERTs into TLOG_AUDIT via SOCI (production)
//   * StdoutLogSink — pretty-prints to spdlog (dev / fallback when DB is down)
//
// The wire decoder feeds whichever sink is wired; switching is a
// main.cpp wire-up change with no impact on the receive path.

#include "fourstory/db/session_pool.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace tlogsvr {

// Decoded _LOG_DATA_ matching the legacy LogPacket.h layout
// (same struct UdpAuditLogger in TLoginSvrAsio emits). All fields
// owned by the value; opaque payload after the search keys is a
// variable-length blob (LF_CHARBASE, LF_ITEM, LF_SKILL, LF_PET).
struct LogRecord
{
    std::string                  timestamp_iso;  // YYYY-MM-DD HH:MM:SS
    std::uint32_t                server_id    = 0;
    std::string                  client_ip;
    std::uint32_t                action       = 0;
    std::uint16_t                map_id       = 0;
    std::int32_t                 pos_x        = 0;
    std::int32_t                 pos_y        = 0;
    std::int32_t                 pos_z        = 0;
    std::int64_t                 search_int[11]{};
    std::string                  search_str[7];
    std::uint32_t                format       = 0;
    std::vector<std::byte>       payload;        // 0..512 raw bytes
};

class ILogSink
{
public:
    virtual ~ILogSink() = default;
    // Persist the record. Synchronous + blocking — UDP receiver is
    // expected to keep up; if DB is slow, drops are preferable to
    // unbounded queueing for an audit channel.
    virtual void Write(const LogRecord& rec) = 0;
};

// SOCI-backed sink writing to LT_* columns in `target_table`.
class SociLogSink : public ILogSink
{
public:
    SociLogSink(fourstory::db::SessionPool& pool, std::string target_table);
    void Write(const LogRecord& rec) override;

private:
    fourstory::db::SessionPool& m_pool;
    std::string                 m_table;
};

// Dev fallback sink — emits each record through spdlog. Useful when
// no DB is configured (audit lands on stderr).
class StdoutLogSink : public ILogSink
{
public:
    void Write(const LogRecord& rec) override;
};

} // namespace tlogsvr
