#pragma once
#include "fourstory/db/session_pool.h"
#include <cstdint>
#include <mutex>
#include <string>

namespace tworldsvr {
struct ReservedMailBatch {
    std::uint8_t delivered{},missing{};
    bool minute_already_processed{},busy{};
};
struct OfflineBillBatch {
    std::uint16_t returned{};
    std::uint32_t missing_senders{};
    bool minute_already_processed{},busy{};
};
// Native PostgreSQL realization of TGetReservedPost. No legacy SQL procedure,
// synthetic item template or in-memory mail storage is used.
class PostgreSQLReservedMail {
public:
    PostgreSQLReservedMail(fourstory::db::SessionPool&,std::uint8_t world,std::string owner_token);
    ReservedMailBatch Deliver(std::int64_t now);
    OfflineBillBatch ReturnOfflineBills(std::int64_t now);
private:
    fourstory::db::SessionPool& m_pool;
    std::uint8_t m_world;
    std::string m_owner_token;
    std::mutex m_mutex;
    bool m_commit_unknown{};
};
}
