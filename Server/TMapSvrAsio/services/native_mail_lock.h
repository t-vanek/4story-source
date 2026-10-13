#pragma once
#include <soci/soci.h>
#include <cstdint>

namespace tmapsvr {
inline void LockNativeMailbox(soci::session& sql,std::uint8_t world,std::uint32_t character) {
    // Separate one-bigint advisory namespace from Login/Map owner pair locks.
    // Serialize inbox admission/count and native block-list changes, without
    // locking another online player's account or checkpoint.
    const long long key=0x504f530000000000LL|(static_cast<long long>(world)<<32)|character;
    long long ignored=0;
    sql << "SELECT 0::bigint FROM (SELECT pg_advisory_xact_lock(CAST(:key AS bigint))) held",
        soci::use(key,"key"),soci::into(ignored);
}
}
