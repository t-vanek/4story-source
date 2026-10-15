#pragma once
#include "rps_repository.h"
#include "friend_repository.h"
#include <atomic>
#include <mutex>
#include <string>

namespace soci {class session;}

namespace tworldsvr {
class PostgreSQLFriendRepository;
class PostgreSQLRpsRepository final:public IRpsRepository {
public:
    explicit PostgreSQLRpsRepository(PostgreSQLFriendRepository&);
    std::vector<TRpsGame> LoadGames() override;
    std::vector<RpsWinDateRow> LoadWinDates() override;
    bool Record(bool,std::uint32_t,std::uint8_t,std::uint8_t,std::int64_t) override;
    bool CommitWin(const RpsRegistry::WinPlan&,std::uint32_t,FriendAuthority,
                   const std::string& map_token,std::uint8_t,std::uint8_t,std::int64_t);
    // Stop also on post-COMMIT cache disagreement; no automatic operation replay.
    void Stop();
private:
    void CheckRelease(soci::session&);
    void WriteRecord(soci::session&,const RpsRegistry::PersistOp&);
    PostgreSQLFriendRepository& m_authority;
    std::mutex m_mutex;
    std::atomic<bool> m_failed{false};
    std::string m_manifest,m_timezone;
    std::vector<TRpsGame> m_games;
    std::vector<RpsWinDateRow> m_dates;
};
}
