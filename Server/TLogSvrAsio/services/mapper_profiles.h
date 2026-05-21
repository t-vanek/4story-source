#pragma once

// MapperProfile bundle for TLogSvrAsio.
//
// Wires the fourstory::mapper framework so handlers + admin tooling
// can flow records through Adapt<>() instead of hand-rolling the
// LogRecord ↔ LogAuditEntry projection at every call site.
//
//   * LogRecord       (UDP-decoded, write-path)
//   * LogAuditEntry   (SELECTed via AuditQueryRepository, read-path)
//
// Both shapes carry the same audit fields; the mapper bridges the
// timestamp + first search key + first search string + payload state
// so a record we just received looks identical to one we'd read back.
//
// Registered at startup via main.cpp:
//   MapperRegistry::Get().Register<LogMappingProfile>();
//   MapperRegistry::Get().ApplyAll();

#include "audit_query_repository.h"
#include "log_sink.h"

#include "fourstory/mapper/mapper.h"

namespace tlogsvr {

class LogMappingProfile : public fourstory::mapper::MapperProfile
{
public:
    const char* Name() const override { return "LogMappingProfile"; }

    void Configure() override
    {
        using namespace fourstory::mapper;

        // LogRecord → LogAuditEntry: project the UDP-decoded record
        // into the same shape the AuditQueryRepository returns. log_id
        // is 0 (assigned by the DB on INSERT); other fields map 1:1
        // from the legacy search key conventions.
        MapperConfig<LogRecord, LogAuditEntry>()
            .Set(&LogAuditEntry::log_date,    &LogRecord::timestamp_iso)
            .Set(&LogAuditEntry::server_id,   &LogRecord::server_id)
            .Set(&LogAuditEntry::client_ip,   &LogRecord::client_ip)
            .Set(&LogAuditEntry::action,      &LogRecord::action)
            .Set(&LogAuditEntry::map_id,      &LogRecord::map_id)
            .Set(&LogAuditEntry::format,      &LogRecord::format)
            // Legacy convention: search_int[0] = dwUserID,
            // search_str[0] = character name. Pull those out so the
            // projected entry matches the read-side EntityMapping.
            .Set(&LogAuditEntry::search_int_0,
                 [](const LogRecord& r) { return r.search_int[0]; })
            .Set(&LogAuditEntry::search_str_0,
                 [](const LogRecord& r) { return r.search_str[0]; })
            .Default(&LogAuditEntry::log_id, std::int64_t{0});

        // LogAuditEntry → LogRecord: reverse projection for replay /
        // test harness paths (rebuilds a wire-shaped record from a
        // DB row). Payload stays empty since the read-side mapping
        // skips the BLOB column.
        MapperConfig<LogAuditEntry, LogRecord>()
            .Set(&LogRecord::timestamp_iso, &LogAuditEntry::log_date)
            .Set(&LogRecord::server_id,     &LogAuditEntry::server_id)
            .Set(&LogRecord::client_ip,     &LogAuditEntry::client_ip)
            .Set(&LogRecord::action,        &LogAuditEntry::action)
            .Set(&LogRecord::map_id,        &LogAuditEntry::map_id)
            .Set(&LogRecord::format,        &LogAuditEntry::format)
            .AfterMap([](const LogAuditEntry& src, LogRecord& dst) {
                dst.search_int[0] = src.search_int_0;
                dst.search_str[0] = src.search_str_0;
            });
    }
};

} // namespace tlogsvr
