#include "services/soci_friend_repository.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

namespace tworldsvr {

FriendLoad SociFriendRepository::LoadForChar(std::uint32_t char_id)
{
    FriendLoad out;
    const int cid = static_cast<int>(char_id);
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;

        // Friend groups (legacy CTBLFriendGroupTable).
        {
            soci::rowset<soci::row> rs = (sql.prepare <<
                "SELECT \"bGroup\", \"szName\" FROM \"TFRIENDGROUPTABLE\" "
                "WHERE \"dwCharID\" = :id",
                soci::use(cid));
            for (const auto& r : rs)
                out.groups.emplace_back(
                    static_cast<std::uint8_t>(r.get<int>("bGroup")),
                    r.get<std::string>("szName"));
        }

        // Forward edges: friends this char added, joined to TCHARTABLE
        // for name / class / level (legacy CTBLFriend).
        {
            soci::rowset<soci::row> rs = (sql.prepare <<
                "SELECT F.\"dwFriendID\", C.\"szName\", F.\"bGroup\", "
                "C.\"bClass\", C.\"bLevel\" "
                "FROM \"TFRIENDTABLE\" AS F "
                "INNER JOIN \"TCHARTABLE\" AS C ON F.\"dwFriendID\" = "
                "C.\"dwCharID\" "
                "WHERE F.\"dwCharID\" = :id AND C.\"bDelete\" = 0",
                soci::use(cid));
            for (const auto& r : rs)
            {
                FriendRow row;
                row.id    = static_cast<std::uint32_t>(r.get<int>("dwFriendID"));
                row.name  = r.get<std::string>("szName");
                row.group = static_cast<std::uint8_t>(r.get<int>("bGroup"));
                row.klass = static_cast<std::uint8_t>(r.get<int>("bClass"));
                row.level = static_cast<std::uint8_t>(r.get<int>("bLevel"));
                out.forward.push_back(std::move(row));
            }
        }

        // Reverse edges: chars who added this char (legacy
        // CTBLFriendTarget — id + name only).
        {
            soci::rowset<soci::row> rs = (sql.prepare <<
                "SELECT F.\"dwCharID\", C.\"szName\" "
                "FROM \"TFRIENDTABLE\" AS F "
                "INNER JOIN \"TCHARTABLE\" AS C ON F.\"dwCharID\" = "
                "C.\"dwCharID\" "
                "WHERE F.\"dwFriendID\" = :id AND C.\"bDelete\" = 0",
                soci::use(cid));
            for (const auto& r : rs)
            {
                FriendRow row;
                row.id   = static_cast<std::uint32_t>(r.get<int>("dwCharID"));
                row.name = r.get<std::string>("szName");
                out.reverse.push_back(std::move(row));
            }
        }

        // Soulmate pairing (legacy TSOULMATETABLE).
        {
            int target = 0, time_unix = 0;
            soci::indicator ti = soci::i_null, tt = soci::i_null;
            sql << "SELECT \"dwTarget\", \"dwTime\" FROM \"TSOULMATETABLE\" "
                   "WHERE \"dwCharID\" = :id",
                soci::use(cid), soci::into(target, ti), soci::into(time_unix, tt);
            if (sql.got_data() && ti == soci::i_ok && target != 0)
            {
                out.has_soulmate    = true;
                out.soulmate_target = static_cast<std::uint32_t>(target);
                out.soulmate_time   = static_cast<std::uint32_t>(
                    tt == soci::i_ok ? time_unix : 0);
            }
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociFriendRepository::LoadForChar({}) failed: {}",
            char_id, ex.what());
        return FriendLoad{};
    }
    return out;
}

} // namespace tworldsvr
