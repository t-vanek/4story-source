#pragma once

// Concrete EntityMapping specialisations for the 4Story global tables.
// Include this header AFTER the entity struct definitions in each
// server's entity header to enable Repository<T> for those types.
//
// This file is documentation-as-code — copy-paste and adapt for game
// entities that live in server-specific headers.

#include "entity_mapping.h"

#include <soci/soci.h>

#include <cstdint>
#include <string>

namespace fourstory::db::orm::examples {

// ── CurrentUser (TCURRENTUSER — session table) ───────────────────────

struct CurrentUser
{
    std::uint32_t dwUserID  = 0;
    std::uint32_t dwKEY     = 0;
    std::string   szLoginIP;
    std::uint8_t  bLocked   = 0;
    std::uint8_t  bGroupID  = 0;
    std::uint8_t  bChannel  = 0;
    std::uint32_t dwCharID  = 0;
};

} // namespace fourstory::db::orm::examples

namespace fourstory::db::orm {

template<>
struct EntityMapping<fourstory::db::orm::examples::CurrentUser>
{
    using T = fourstory::db::orm::examples::CurrentUser;

    static constexpr const char* Table    = "TCURRENTUSER";
    static constexpr const char* PkColumn = "dwUserID";

    static T FromRow(const soci::row& r)
    {
        T u;
        u.dwUserID  = static_cast<std::uint32_t>(r.get<int>(0));
        u.dwKEY     = static_cast<std::uint32_t>(r.get<int>(1));
        u.szLoginIP = r.get<std::string>(2);
        u.bLocked   = static_cast<std::uint8_t>(r.get<int>(3));
        u.bGroupID  = static_cast<std::uint8_t>(r.get<int>(4));
        u.bChannel  = static_cast<std::uint8_t>(r.get<int>(5));
        u.dwCharID  = static_cast<std::uint32_t>(r.get<int>(6));
        return u;
    }

    static std::string SelectAllSql()
    {
        return "SELECT dwUserID, dwKEY, szLoginIP, bLocked, "
               "bGroupID, bChannel, dwCharID FROM TCURRENTUSER";
    }
    static std::string SelectByIdSql()
    {
        return SelectAllSql() + " WHERE dwUserID = :pk";
    }
    static std::string InsertSql()
    {
        return "INSERT INTO TCURRENTUSER "
               "(dwUserID, dwKEY, szLoginIP, bLocked, bGroupID, bChannel, dwCharID) "
               "VALUES (:uid, :key, :ip, :locked, :group, :chan, :char_id)";
    }
    static std::string UpdateSql()
    {
        return "UPDATE TCURRENTUSER SET "
               "dwKEY=:key, szLoginIP=:ip, bLocked=:locked, "
               "bGroupID=:group, bChannel=:chan, dwCharID=:char_id "
               "WHERE dwUserID=:pk";
    }
    static std::string DeleteSql()
    {
        return "DELETE FROM TCURRENTUSER WHERE dwUserID = :pk";
    }
    static void BindInsert(soci::statement& st, const T& e)
    {
        const int uid = e.dwUserID, key = e.dwKEY, locked = e.bLocked,
                  grp = e.bGroupID, chan = e.bChannel, cid = e.dwCharID;
        st , soci::use(uid,       "uid")
           , soci::use(key,       "key")
           , soci::use(e.szLoginIP, "ip")
           , soci::use(locked,    "locked")
           , soci::use(grp,       "group")
           , soci::use(chan,      "chan")
           , soci::use(cid,       "char_id");
    }
    static void BindUpdate(soci::statement& st, const T& e)
    {
        const int uid = e.dwUserID, key = e.dwKEY, locked = e.bLocked,
                  grp = e.bGroupID, chan = e.bChannel, cid = e.dwCharID;
        st , soci::use(key,       "key")
           , soci::use(e.szLoginIP, "ip")
           , soci::use(locked,    "locked")
           , soci::use(grp,       "group")
           , soci::use(chan,      "chan")
           , soci::use(cid,       "char_id")
           , soci::use(uid,       "pk");
    }
    static int GetPk(const T& e) { return static_cast<int>(e.dwUserID); }
};

} // namespace fourstory::db::orm
