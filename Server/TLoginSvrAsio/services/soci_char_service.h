#pragma once

// SOCI-backed ICharService. Two-pool design matching legacy schema split:
//   global (TGLOBAL)  — TALLCHARTABLE, TKEEPINGNAME, TRESERVEDNAME, TVETERANCHART
//   world  (TGAME)    — TCHARTABLE, TITEMTABLE, TGUILDMEMBERTABLE
//
// Tables:
//   * TCHARTABLE        — per-world character row, full attrs (world pool).
//   * TALLCHARTABLE     — cross-world directory (one row per char per
//                         world). Lives in TGLOBAL so a single login server
//                         can serve users across multiple world DBs.
//   * TKEEPINGNAME      — name-reservation table (e.g. dead-char names
//                         kept on cooldown). Global pool.
//   * TRESERVEDNAME     — admin-reserved names. Global pool.
//   * TGUILDMEMBERTABLE — guild membership row blocks Delete (legacy
//                         TDeleteChar behavior). World pool.
//   * TITEMTABLE        — starter inventory inserts on Create. World pool.
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

#include <mutex>
#include <unordered_map>

namespace fourstory::db { class SessionPool; }

namespace tloginsvr::services {

class SociCharService : public ICharService
{
public:
    // Loads the TVETERANCHART rows into an in-memory cache at construction
    // time (matches legacy CTLoginSvrModule init at TLoginSvr.cpp:382 — the
    // chart is small and effectively read-only, so reloading per-request
    // would just add DB load). If the table is missing or unreadable the
    // service falls back to "no veteran bonus" silently.
    //
    // `global_pool` — TGLOBAL (accounts + cross-world directory tables).
    // `world_pool`  — TGAME (per-world tables). Lifetimes must exceed
    //   this service. Both required for real character ops.
    SociCharService(fourstory::db::SessionPool& global_pool, fourstory::db::SessionPool& world_pool);

    std::vector<CharacterInfo>
    List(std::int32_t user_id, std::uint8_t group_id) override;

    CharacterCreateResponse
    Create(const CharacterCreateRequest& req) override;

    DeleteCharResult
    Delete(std::int32_t user_id,
           std::uint8_t group_id,
           std::int32_t char_id,
           const std::string& password) override;

    // Returns the first three rows of TVETERANCHART (cached at ctor).
    // Empty cache → all zeros.
    VeteranLevels GetVeteranLevels() const override;

    // BR shard char lookup via TBRPLAYERTABLE (per-world TGAME).
    // Returns the dwCharID enrolled, or 0 on miss / DB error /
    // missing table.
    std::int32_t GetBrCharId(std::int32_t user_id) override;

    // Reload TVETERANCHART into the in-memory cache. Wired into
    // RegistryRefresher so an operator running an UPDATE on the
    // chart table sees the new thresholds within one tick.
    void RefreshVeteranChart();

private:
    fourstory::db::SessionPool& m_global; // TGLOBAL
    fourstory::db::SessionPool& m_world;  // TGAME (per-world)

    // m_veteran_levels guarded against RefreshVeteranChart racing
    // GetVeteranLevels — both lock m_veteran_mtx.
    mutable std::mutex m_veteran_mtx;

    // m_veteran_levels: bLevelOption → bLevel. Empty means no veteran
    // chart was loaded (CR_SUCCESS still works, just defaults to the
    // country-based starting level).
    std::unordered_map<std::uint8_t, std::uint8_t> m_veteran_levels;
};

} // namespace tloginsvr::services
