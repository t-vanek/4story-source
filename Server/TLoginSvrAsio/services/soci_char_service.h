#pragma once

// SOCI-backed ICharService. Schema layer:
//   * TCHARTABLE        — per-world character row, full attrs.
//   * TALLCHARTABLE     — cross-world directory (one row per char per
//                         world). Lets us implement (user, world)-scoped
//                         List against a single PG/MSSQL instance even
//                         though the legacy server used DB-per-world.
//   * TKEEPINGNAME      — name-reservation table (e.g. dead-char names
//                         kept on cooldown).
//   * TRESERVEDNAME     — admin-reserved names.
//   * TGUILDMEMBERTABLE — guild membership row blocks Delete (legacy
//                         TDeleteChar behavior).
//
// Delete semantics — matches the legacy TDeleteChar SP:
//   * If TGUILDMEMBERTABLE has a row for dwCharID → DR_FAILED (in-guild).
//   * If bLevel > 5: soft delete (bDelete=1, dDeleteDate=now) in both
//     TCHARTABLE + TALLCHARTABLE. Name becomes available again because
//     the legacy uniqueness index is partial (WHERE bDelete=0).
//   * If bLevel <= 5: hard delete from TCHARTABLE; mark soft-deleted in
//     TALLCHARTABLE so audit can still reconstruct.
//
// Password validation for Delete is NOT performed here — the handler
// routes the request through IAuthService::Authenticate first (matches
// legacy CSHandler dispatch where TLoginPasswordCheck runs before
// TDeleteChar). Phase B keeps that split.

#include "char_service.h"

#include <unordered_map>

namespace tloginsvr::db { class SessionPool; }

namespace tloginsvr::services {

class SociCharService : public ICharService
{
public:
    // Loads the TVETERANCHART rows into an in-memory cache at construction
    // time (matches legacy CTLoginSvrModule init at TLoginSvr.cpp:382 — the
    // chart is small and effectively read-only, so reloading per-request
    // would just add DB load). If the table is missing or unreadable the
    // service falls back to "no veteran bonus" silently.
    explicit SociCharService(db::SessionPool& pool);

    std::vector<CharacterInfo>
    List(std::int32_t user_id, std::uint8_t group_id) override;

    CharacterCreateResponse
    Create(const CharacterCreateRequest& req) override;

    DeleteCharResult
    Delete(std::int32_t user_id,
           std::uint8_t group_id,
           std::int32_t char_id,
           const std::string& password) override;

private:
    db::SessionPool& m_pool;

    // m_veteran_levels: bLevelOption → bLevel. Empty means no veteran
    // chart was loaded (CR_SUCCESS still works, just defaults to the
    // country-based starting level).
    std::unordered_map<std::uint8_t, std::uint8_t> m_veteran_levels;
};

} // namespace tloginsvr::services
