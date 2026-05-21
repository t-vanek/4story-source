// Boot-time schema validator for TMapSvrAsio — fail-fast on the
// session table that the F4 handshake reads.
//
// Column list mirrors what TLoginSvrAsio writes when a login succeeds
// (dwKEY, dwUserID, szLoginIP, bLocked, bGroupID, bChannel). The map
// server consumes the same row to validate CS_VERIFYSESSION_REQ: it
// looks the row up by (dwUserID, dwKEY) and confirms the channel /
// group match what the client claims.
//
// Char-table validation (TCHARTABLE columns for F5 load/save) is
// intentionally deferred — F2 should boot against a minimal dev DB
// that only carries the session table.

#include "schema_validator.h"

#include "fourstory/db/schema_validator.h"
#include "fourstory/db/session_pool.h"

namespace tmapsvr::db {

void ValidateUserSchema(fourstory::db::SessionPool& pool)
{
    auto lease = pool.Acquire();

    // Session row TLoginSvrAsio writes on successful login. The F4
    // handshake handler reads (dwUserID, dwKEY) for the auth check
    // and (bGroupID, bChannel, szLoginIP, bLocked) for the session
    // claims the client passes in CS_VERIFYSESSION_REQ.
    fourstory::db::CheckColumns(*lease, "map_user", {
        { "TCURRENTUSER", "dwKEY" },
        { "TCURRENTUSER", "dwUserID" },
        { "TCURRENTUSER", "szLoginIP" },
        { "TCURRENTUSER", "bLocked" },
        { "TCURRENTUSER", "bGroupID" },
        { "TCURRENTUSER", "bChannel" },
    });
}

void ValidateCharSchema(fourstory::db::SessionPool& pool)
{
    auto lease = pool.Acquire();

    // TCHARTABLE columns the F8 player service reads when handling
    // DM_LOADCHAR_REQ — mirror what's SELECTed in
    // SociPlayerService::LoadChar. bDelete is here too because the
    // WHERE clause filters on it. Future phases (items / skills /
    // quests / equip) add their own tables in sibling validators.
    fourstory::db::CheckColumns(*lease, "map_char", {
        { "TCHARTABLE", "dwCharID" },
        { "TCHARTABLE", "szNAME" },
        { "TCHARTABLE", "bDelete" },
        { "TCHARTABLE", "bStartAct" },
        { "TCHARTABLE", "bRealSex" },
        { "TCHARTABLE", "bClass" },
        { "TCHARTABLE", "bLevel" },
        { "TCHARTABLE", "bRace" },
        { "TCHARTABLE", "bCountry" },
        { "TCHARTABLE", "bOriCountry" },
        { "TCHARTABLE", "bSex" },
        { "TCHARTABLE", "bHair" },
        { "TCHARTABLE", "bFace" },
        { "TCHARTABLE", "bBody" },
        { "TCHARTABLE", "bPants" },
        { "TCHARTABLE", "bHand" },
        { "TCHARTABLE", "bFoot" },
        { "TCHARTABLE", "bHelmetHide" },
        { "TCHARTABLE", "dwGold" },
        { "TCHARTABLE", "dwSilver" },
        { "TCHARTABLE", "dwCooper" },
        { "TCHARTABLE", "dwEXP" },
        { "TCHARTABLE", "dwHP" },
        { "TCHARTABLE", "dwMP" },
        { "TCHARTABLE", "wSkillPoint" },
        { "TCHARTABLE", "dwRegion" },
        { "TCHARTABLE", "bGuildLeave" },
        { "TCHARTABLE", "dwGuildLeaveTime" },
        { "TCHARTABLE", "wMapID" },
        { "TCHARTABLE", "wSpawnID" },
        { "TCHARTABLE", "wLastSpawnID" },
        { "TCHARTABLE", "dwLastDestination" },
        { "TCHARTABLE", "wTemptedMon" },
        { "TCHARTABLE", "bAftermath" },
        { "TCHARTABLE", "fPosX" },
        { "TCHARTABLE", "fPosY" },
        { "TCHARTABLE", "fPosZ" },
        { "TCHARTABLE", "wDIR" },
        { "TCHARTABLE", "bStatLevel" },
        { "TCHARTABLE", "bStatPoint" },
        { "TCHARTABLE", "dwStatExp" },
    });
}

} // namespace tmapsvr::db
