#pragma once

// IAuditLogger — operational + forensic event sink for the login flow.
// Maps 1:1 onto the legacy TLogSvr UDP packets emitted from
// Server/TLoginSvr/UdpSocket.cpp::LogLogin / LogCharCreate /
// LogCharDelete / LogGameStart, but the modern transport is "log
// records emitted via spdlog" so they ship over the same channel as
// the rest of the server's logs (Seq, Loki, ELK, …).
//
// All methods are synchronous and must be safe to call from the Asio
// io_context — implementations should NOT block on disk / network
// (spdlog default sinks are async-friendly enough; a syslog or
// network sink would need its own queue).

#include <cstdint>
#include <string>
#include <string_view>

namespace fourstory::audit {

// Outcome of a CS_LOGIN_REQ. Maps to the wire LR_* codes; named here so
// the audit-log consumer can render them without the wire-format dep.
enum class LoginOutcome
{
    Success,
    NoUser,
    WrongPassword,
    Duplicate,
    Banned,
    IpBanned,
    AgreementNeeded,
    VersionMismatch,
    InternalError,
};

// Outcome of a CS_DELCHAR_REQ. Mirrors the legacy DR_* family.
enum class DeleteCharOutcome
{
    Success,
    Failed,
    InvalidPassword,
    Internal,
    NoGroup,
};

// Outcome of a CS_CREATECHAR_REQ. Mirrors legacy CR_*.
enum class CreateCharOutcome
{
    Success,
    NoGroup,
    DuplicateName,
    InvalidSlot,
    Protected,
    OverChar,
    Internal,
};

class IAuditLogger
{
public:
    virtual ~IAuditLogger() = default;

    // Login attempt — fired regardless of outcome. `user_id_str` is
    // the raw user_id the client claimed (may be junk if the attempt
    // is mid-attack); `db_user_id` is the resolved dwUserID (0 if
    // the user wasn't found or the attempt never got to the DB).
    virtual void LogLogin(LoginOutcome outcome,
                          const std::string& user_id_str,
                          std::int32_t db_user_id,
                          const std::string& client_ip,
                          std::uint32_t session_key) = 0;

    // Character create — fired with the chosen name + the result code.
    // db_char_id is 0 unless the row was actually inserted.
    virtual void LogCharCreate(CreateCharOutcome outcome,
                               std::int32_t db_user_id,
                               std::uint8_t group_id,
                               const std::string& char_name,
                               std::int32_t db_char_id) = 0;

    // Character delete — db_char_id is the id the client requested;
    // present in both success + failure logs so an op can audit "who
    // tried to delete what."
    virtual void LogCharDelete(DeleteCharOutcome outcome,
                               std::int32_t db_user_id,
                               std::uint8_t group_id,
                               std::int32_t db_char_id) = 0;

    // Map server hand-off — `success` is true on SR_SUCCESS, false on
    // SR_NOSERVER. Records the resolved server_id for ops correlation.
    virtual void LogGameStart(bool success,
                              std::int32_t db_user_id,
                              std::uint8_t group_id,
                              std::uint8_t channel,
                              std::int32_t db_char_id,
                              std::uint8_t map_server_id) = 0;
};

} // namespace fourstory::audit
