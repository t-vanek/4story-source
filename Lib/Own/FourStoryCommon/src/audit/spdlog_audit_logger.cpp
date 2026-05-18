#include "fourstory/audit/spdlog_audit_logger.h"

#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace fourstory::audit {

namespace {

const char* Name(LoginOutcome o)
{
    switch (o)
    {
    case LoginOutcome::Success:         return "Success";
    case LoginOutcome::NoUser:          return "NoUser";
    case LoginOutcome::WrongPassword:   return "WrongPassword";
    case LoginOutcome::Duplicate:       return "Duplicate";
    case LoginOutcome::Banned:          return "Banned";
    case LoginOutcome::IpBanned:        return "IpBanned";
    case LoginOutcome::AgreementNeeded: return "AgreementNeeded";
    case LoginOutcome::VersionMismatch: return "VersionMismatch";
    case LoginOutcome::InternalError:   return "InternalError";
    }
    return "?";
}

const char* Name(DeleteCharOutcome o)
{
    switch (o)
    {
    case DeleteCharOutcome::Success:         return "Success";
    case DeleteCharOutcome::Failed:          return "Failed";
    case DeleteCharOutcome::InvalidPassword: return "InvalidPassword";
    case DeleteCharOutcome::Internal:        return "Internal";
    case DeleteCharOutcome::NoGroup:         return "NoGroup";
    }
    return "?";
}

const char* Name(CreateCharOutcome o)
{
    switch (o)
    {
    case CreateCharOutcome::Success:        return "Success";
    case CreateCharOutcome::NoGroup:        return "NoGroup";
    case CreateCharOutcome::DuplicateName:  return "DuplicateName";
    case CreateCharOutcome::InvalidSlot:    return "InvalidSlot";
    case CreateCharOutcome::Protected:      return "Protected";
    case CreateCharOutcome::OverChar:       return "OverChar";
    case CreateCharOutcome::Internal:       return "Internal";
    }
    return "?";
}

} // namespace

SpdlogAuditLogger::SpdlogAuditLogger()
{
    // Reuse a previously-registered "audit" logger if one exists
    // (production deploys can set up a structured sink ahead of
    // service construction). Otherwise spin up one sharing the
    // default-logger sinks, so the audit stream is at least visible
    // on stderr alongside the rest.
    m_logger = spdlog::get("audit");
    if (!m_logger)
    {
        auto default_logger = spdlog::default_logger();
        if (default_logger && !default_logger->sinks().empty())
        {
            m_logger = std::make_shared<spdlog::logger>(
                "audit",
                default_logger->sinks().begin(),
                default_logger->sinks().end());
        }
        else
        {
            // Last-resort fallback — bare stderr color sink.
            m_logger = spdlog::stderr_color_mt("audit");
        }
        spdlog::register_logger(m_logger);
    }
    m_logger->set_level(spdlog::level::info);
}

void SpdlogAuditLogger::LogLogin(LoginOutcome outcome,
                                 const std::string& user_id_str,
                                 std::int32_t db_user_id,
                                 const std::string& client_ip,
                                 std::uint32_t session_key)
{
    m_logger->info("event=login outcome={} uid_str='{}' uid={} ip={} key=0x{:08X}",
        Name(outcome), user_id_str, db_user_id, client_ip, session_key);
}

void SpdlogAuditLogger::LogCharCreate(CreateCharOutcome outcome,
                                      std::int32_t db_user_id,
                                      std::uint8_t group_id,
                                      const std::string& char_name,
                                      std::int32_t db_char_id)
{
    m_logger->info("event=char_create outcome={} uid={} group={} name='{}' char_id={}",
        Name(outcome), db_user_id, static_cast<int>(group_id), char_name, db_char_id);
}

void SpdlogAuditLogger::LogCharDelete(DeleteCharOutcome outcome,
                                      std::int32_t db_user_id,
                                      std::uint8_t group_id,
                                      std::int32_t db_char_id)
{
    m_logger->info("event=char_delete outcome={} uid={} group={} char_id={}",
        Name(outcome), db_user_id, static_cast<int>(group_id), db_char_id);
}

void SpdlogAuditLogger::LogGameStart(bool success,
                                     std::int32_t db_user_id,
                                     std::uint8_t group_id,
                                     std::uint8_t channel,
                                     std::int32_t db_char_id,
                                     std::uint8_t map_server_id)
{
    m_logger->info("event=game_start success={} uid={} group={} ch={} char={} svr={}",
        success ? 1 : 0, db_user_id, static_cast<int>(group_id),
        static_cast<int>(channel), db_char_id, static_cast<int>(map_server_id));
}

} // namespace fourstory::audit
