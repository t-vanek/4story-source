// Boot-time schema validator for TMapSvrAsio (F2 scope).
//
// F2 only reads TCURRENTUSER. Later phases will extend the required
// column list as gameplay handlers land:
//   * F2: TCURRENTUSER (this file)
//   * F3+: TCHARTABLE, TITEMTABLE, TGUILDMEMBERTABLE, TGUILDTABLE,
//          TSPAWNPOSCHART, TMAPCHART, …
//
// Mirrors the validator shape used by TLoginSvrAsio and TPatchSvrAsio
// — fail-fast on missing columns so a schema drift surfaces at boot
// rather than on the first client request.

#include "schema_validator.h"

#include "fourstory/db/schema_validator.h"
#include "fourstory/db/session_pool.h"

namespace tmapsvr::db {

void ValidateGlobalSchema(fourstory::db::SessionPool& pool)
{
    auto lease = pool.Acquire();
    fourstory::db::CheckColumns(*lease, "map_global", {
        // SociMapSessionValidator::Validate reads (dwUserID, dwCharID,
        // dwKEY) on every CS_CONNECT_REQ. The other TCURRENTUSER
        // columns (bGroupID, bChannel, szLoginIP, …) are read by F3+
        // handlers — they'll be added to this list when those handlers
        // land.
        { "TCURRENTUSER", "dwUserID" },
        { "TCURRENTUSER", "dwCharID" },
        { "TCURRENTUSER", "dwKEY" },
    });
}

} // namespace tmapsvr::db
