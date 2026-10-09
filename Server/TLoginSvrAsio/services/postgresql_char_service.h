#pragma once
#include "char_service.h"
#include "fourstory/db/session_pool.h"

namespace tloginsvr::services {
// Native normal-world lobby. One DB owns global directory and every world row.
// The selected reference release is checked for every transaction. No historical
// schema is writable by this service; backup procedures define starter values.
class PostgreSQLCharService final : public ICharService {
public:
    PostgreSQLCharService(fourstory::db::SessionPool& pool, std::string owner,
                          std::string manifest);
    std::vector<CharacterInfo> List(std::int32_t user, std::uint8_t world) override;
    CharacterCreateResponse Create(const CharacterCreateRequest& req) override;
    DeleteCharResult Delete(std::int32_t, std::uint8_t, std::int32_t, const std::string&) override {
        return DeleteCharResult::Internal; // Native mutations require the authenticated session key.
    }
    DeleteCharResult DeleteAuthorized(std::int32_t user, std::uint8_t world, std::int32_t character,
                                      const std::string& password, std::uint32_t key) override;
    VeteranLevels GetVeteranLevels() const override { return m_veterans; }
    // Only normal worlds may be provisioned for this slice. BR/BOW remain disabled.
    std::int32_t GetBrCharId(std::int32_t) override { return 0; }
    std::int32_t GetBowCharId(std::int32_t) override { return 0; }
private:
    void CheckRelease(soci::session& sql) const;
    bool LockSession(soci::session& sql, int user, std::uint32_t key) const;
    bool HasWorld(soci::session& sql, int world) const;
    CharacterCreateResponse CreateOnce(const CharacterCreateRequest& req);
    fourstory::db::SessionPool& m_pool;
    std::string m_owner, m_manifest;
    VeteranLevels m_veterans{};
};
}
