#include "services/soci_friend_repository.h"

#include "services/friend_entities.h"

#include "fourstory/db/orm/db_context.h"
#include "fourstory/mapper/mapper.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>
#include <bit>

namespace tworldsvr {

using fourstory::db::orm::DbContext;
using fourstory::mapper::AdaptAll;

void SociFriendRepository::CheckWritable() const {
    if(m_pair_failed.load())throw std::runtime_error("Friend persistence stopped for reconciliation");
}

bool SociFriendRepository::InsertMutualFriends(std::uint32_t char_id,std::uint32_t friend_id) {
    std::lock_guard guard(m_pair_mutex);CheckWritable();
    try {
        DbContext ctx(m_pool);auto& sql=ctx.Session();soci::transaction tx(sql);
        const std::int32_t a=std::bit_cast<std::int32_t>(char_id),b=std::bit_cast<std::int32_t>(friend_id);
        // This is the existing schema adapter, not a claim of native World
        // bootstrap. Keep both source TFriendInsert side effects in one commit.
        const auto insert=[&](int from,int to) {
            int exists=0;
            sql << "SELECT count(*) FROM \"TFRIENDTABLE\" WHERE \"dwCharID\"=:c AND \"dwFriendID\"=:f",
                soci::use(from,"c"),soci::use(to,"f"),soci::into(exists);
            if(!exists)sql << "INSERT INTO \"TFRIENDTABLE\"(\"dwCharID\",\"dwFriendID\",\"bGroup\") VALUES(:c,:f,0)",
                soci::use(from,"c"),soci::use(to,"f");
        };
        if(char_id<friend_id){insert(a,b);insert(b,a);}
        else {insert(b,a);insert(a,b);}
        // Lost COMMIT is never converted to false followed by an automatic
        // repeat. Any persistence failure stops this instance and the World.
        tx.commit();return true;
    }catch(...) {
        m_pair_failed.store(true);
        if(m_stop_on_failure)m_stop_on_failure();
        throw std::runtime_error("Mutual friend persistence failed; reconciliation required");
    }
}

FriendLoad SociFriendRepository::LoadForChar(std::uint32_t char_id)
{
    FriendLoad out;
    const int cid = static_cast<int>(char_id);
    try
    {
        // One DbContext = one pooled session for all four reads. The
        // forward/reverse edges are parameterized JOINs against
        // TCHARTABLE that don't fit the single-table Repository<T> shape,
        // so they run through the shared session directly — same raw
        // escape hatch the guild repo's point-log pass uses.
        DbContext ctx(m_pool);
        soci::session& sql = ctx.Session();

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
        // for name / class / level (legacy CTBLFriend). The ORM maps each
        // JOIN row → FriendForwardRow, the Automapper folds it → FriendRow.
        out.forward = AdaptAll<FriendRow>(
            ctx.Set<FriendForwardRow>().QueryBound(
                "SELECT F.\"dwFriendID\", C.\"szName\", F.\"bGroup\", "
                "C.\"bClass\", C.\"bLevel\" "
                "FROM \"TFRIENDTABLE\" AS F "
                "INNER JOIN \"TCHARTABLE\" AS C ON F.\"dwFriendID\" = "
                "C.\"dwCharID\" "
                "WHERE F.\"dwCharID\" = :id AND C.\"bDelete\" = 0",
                soci::use(cid)));

        // Reverse edges: chars who added this char (legacy
        // CTBLFriendTarget — id + name only).
        out.reverse = AdaptAll<FriendRow>(
            ctx.Set<FriendReverseRow>().QueryBound(
                "SELECT F.\"dwCharID\", C.\"szName\" "
                "FROM \"TFRIENDTABLE\" AS F "
                "INNER JOIN \"TCHARTABLE\" AS C ON F.\"dwCharID\" = "
                "C.\"dwCharID\" "
                "WHERE F.\"dwFriendID\" = :id AND C.\"bDelete\" = 0",
                soci::use(cid)));

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

bool SociFriendRepository::MakeGroup(std::uint32_t char_id, std::uint8_t group,
                                     const std::string& name)
{
    CheckWritable();
    try
    {
        DbContext ctx(m_pool);
        const int bound_char_id_1 = static_cast<int>(char_id);
        const int bound_group_2 = static_cast<int>(group);
        ctx.Session() << "INSERT INTO \"TFRIENDGROUPTABLE\" "
                  "(\"dwCharID\", \"bGroup\", \"szName\") "
                  "VALUES (:c, :g, :n)",
            soci::use(bound_char_id_1),
            soci::use(bound_group_2), soci::use(name);
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociFriendRepository::MakeGroup({},{}) failed: {}",
            char_id, group, ex.what());
        return false;
    }
}

bool SociFriendRepository::DeleteGroup(std::uint32_t char_id,
                                       std::uint8_t group)
{
    CheckWritable();
    try
    {
        DbContext ctx(m_pool);
        const int bound_char_id_3 = static_cast<int>(char_id);
        const int bound_group_4 = static_cast<int>(group);
        ctx.Session() << "DELETE FROM \"TFRIENDGROUPTABLE\" "
                  "WHERE \"dwCharID\" = :c AND \"bGroup\" = :g",
            soci::use(bound_char_id_3),
            soci::use(bound_group_4);
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociFriendRepository::DeleteGroup({},{}) failed: {}",
            char_id, group, ex.what());
        return false;
    }
}

bool SociFriendRepository::RenameGroup(std::uint32_t char_id,
                                       std::uint8_t group,
                                       const std::string& name)
{
    CheckWritable();
    try
    {
        DbContext ctx(m_pool);
        const int bound_char_id_5 = static_cast<int>(char_id);
        const int bound_group_6 = static_cast<int>(group);
        ctx.Session() << "UPDATE \"TFRIENDGROUPTABLE\" SET \"szName\" = :n "
                  "WHERE \"dwCharID\" = :c AND \"bGroup\" = :g",
            soci::use(name), soci::use(bound_char_id_5),
            soci::use(bound_group_6);
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociFriendRepository::RenameGroup({},{}) failed: {}",
            char_id, group, ex.what());
        return false;
    }
}

bool SociFriendRepository::ChangeFriendGroup(std::uint32_t char_id,
                                             std::uint32_t friend_id,
                                             std::uint8_t group)
{
    CheckWritable();
    try
    {
        DbContext ctx(m_pool);
        const int bound_group_7 = static_cast<int>(group);
        const int bound_char_id_8 = static_cast<int>(char_id);
        const int bound_friend_id_9 = static_cast<int>(friend_id);
        ctx.Session() << "UPDATE \"TFRIENDTABLE\" SET \"bGroup\" = :g "
                  "WHERE \"dwCharID\" = :c AND \"dwFriendID\" = :f",
            soci::use(bound_group_7),
            soci::use(bound_char_id_8),
            soci::use(bound_friend_id_9);
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociFriendRepository::ChangeFriendGroup({},{}) "
            "failed: {}", char_id, friend_id, ex.what());
        return false;
    }
}

bool SociFriendRepository::InsertFriend(std::uint32_t char_id,
                                        std::uint32_t friend_id)
{
    CheckWritable();
    try
    {
        DbContext ctx(m_pool);
        const int bound_char_id_10 = static_cast<int>(char_id);
        const int bound_friend_id_11 = static_cast<int>(friend_id);
        ctx.Session() << "INSERT INTO \"TFRIENDTABLE\" "
                  "(\"dwCharID\", \"dwFriendID\", \"bGroup\") "
                  "VALUES (:c, :f, 0)",
            soci::use(bound_char_id_10),
            soci::use(bound_friend_id_11);
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociFriendRepository::InsertFriend({},{}) failed: {}",
            char_id, friend_id, ex.what());
        return false;
    }
}

bool SociFriendRepository::EraseFriend(std::uint32_t char_id,
                                       std::uint32_t friend_id)
{
    CheckWritable();
    try
    {
        DbContext ctx(m_pool);
        const int bound_char_id_12 = static_cast<int>(char_id);
        const int bound_friend_id_13 = static_cast<int>(friend_id);
        ctx.Session() << "DELETE FROM \"TFRIENDTABLE\" "
                  "WHERE \"dwCharID\" = :c AND \"dwFriendID\" = :f",
            soci::use(bound_char_id_12),
            soci::use(bound_friend_id_13);
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociFriendRepository::EraseFriend({},{}) failed: {}",
            char_id, friend_id, ex.what());
        return false;
    }
}

bool SociFriendRepository::RegSoulmate(std::uint32_t char_id,
                                       std::uint32_t target)
{
    CheckWritable();
    try
    {
        DbContext ctx(m_pool);
        soci::session& sql = ctx.Session();
        // dwCharID is the PK — delete-then-insert is a portable upsert
        // with the same net effect as TSoulmateReg (one row, dwTime 0).
        const int bound_char_id_14 = static_cast<int>(char_id);
        sql << "DELETE FROM \"TSOULMATETABLE\" WHERE \"dwCharID\" = :c",
            soci::use(bound_char_id_14);
        const int bound_char_id_15 = static_cast<int>(char_id);
        const int bound_target_16 = static_cast<int>(target);
        sql << "INSERT INTO \"TSOULMATETABLE\" "
               "(\"dwCharID\", \"dwTarget\", \"dwTime\") VALUES (:c, :t, 0)",
            soci::use(bound_char_id_15),
            soci::use(bound_target_16);
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociFriendRepository::RegSoulmate({},{}) failed: {}",
            char_id, target, ex.what());
        return false;
    }
}

bool SociFriendRepository::DelSoulmate(std::uint32_t char_id,
                                       std::uint32_t target)
{
    CheckWritable();
    try
    {
        DbContext ctx(m_pool);
        const int bound_char_id_17 = static_cast<int>(char_id);
        const int bound_target_18 = static_cast<int>(target);
        ctx.Session() << "DELETE FROM \"TSOULMATETABLE\" "
                  "WHERE \"dwCharID\" = :c AND \"dwTarget\" = :t",
            soci::use(bound_char_id_17),
            soci::use(bound_target_18);
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociFriendRepository::DelSoulmate({},{}) failed: {}",
            char_id, target, ex.what());
        return false;
    }
}

} // namespace tworldsvr
