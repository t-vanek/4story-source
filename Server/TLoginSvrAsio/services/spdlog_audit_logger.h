#pragma once

// SpdlogAuditLogger — default IAuditLogger implementation. Emits one
// log record per audit event through a dedicated spdlog logger named
// "audit". Shipping to a structured sink (Seq, Loki, …) is then a
// matter of attaching a sink to this logger in main, separate from
// the rest of the server's logs.
//
// Each event line is a single-line key=value record, e.g.:
//   [audit] event=login outcome=Success uid_str='dev' uid=2 ip=127.0.0.1 key=0x00001234
//   [audit] event=char_create outcome=Success uid=2 group=1 name='Hero' char_id=12345
//   [audit] event=char_delete outcome=Success uid=2 group=1 char_id=12345
//   [audit] event=game_start success=1 uid=2 group=1 ch=1 char=12345 svr=1
//
// The key=value shape is grep-friendly, parses cleanly with Loki's
// logfmt parser, and stays human-readable on stderr.

#include "audit_logger.h"

#include <memory>

namespace spdlog { class logger; }

namespace tloginsvr::services {

class SpdlogAuditLogger : public IAuditLogger
{
public:
    // Constructs / fetches the "audit" logger on first call. If the
    // global registry doesn't have one, a new one is created sharing
    // the default sinks (stderr by default). Production deployments
    // can register a custom "audit" logger before constructing this
    // service to redirect just the audit stream to a structured sink.
    SpdlogAuditLogger();

    void LogLogin(LoginOutcome outcome,
                  const std::string& user_id_str,
                  std::int32_t db_user_id,
                  const std::string& client_ip,
                  std::uint32_t session_key) override;

    void LogCharCreate(CreateCharOutcome outcome,
                       std::int32_t db_user_id,
                       std::uint8_t group_id,
                       const std::string& char_name,
                       std::int32_t db_char_id) override;

    void LogCharDelete(DeleteCharOutcome outcome,
                       std::int32_t db_user_id,
                       std::uint8_t group_id,
                       std::int32_t db_char_id) override;

    void LogGameStart(bool success,
                      std::int32_t db_user_id,
                      std::uint8_t group_id,
                      std::uint8_t channel,
                      std::int32_t db_char_id,
                      std::uint8_t map_server_id) override;

private:
    std::shared_ptr<spdlog::logger> m_logger;
};

} // namespace tloginsvr::services
