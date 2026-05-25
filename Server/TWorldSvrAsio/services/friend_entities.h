#pragma once

// Friend DB-row entities + EntityMapping<> specializations for the two
// TFRIENDTABLE⋈TCHARTABLE social-graph reads. The forward and reverse
// edges select different column sets but both fold into the domain
// FriendRow (friend_repository.h) via the Automapper profile in
// mapper_profiles.h:
//
//   forward JOIN row → FriendForwardRow ─(Adapt)→ FriendRow
//   reverse JOIN row → FriendReverseRow ─(Adapt)→ FriendRow
//
// These are parameterized JOINs (WHERE … = :id), so SociFriendRepository
// runs them via Repository<T>::QueryBound(sql, soci::use(id)) — the ORM
// still maps each row through FromRow, no hand-written rowset loop.

#include "fourstory/db/orm/entity_mapping.h"

#include <soci/soci.h>

#include <cstdint>
#include <string>

namespace tworldsvr {

// Forward edge: a friend this char added, joined to TCHARTABLE for the
// display fields (legacy CTBLFriend).
struct FriendForwardRow
{
    std::uint32_t friend_id = 0;   // F.dwFriendID
    std::string   name;            // C.szName
    std::uint8_t  group     = 0;   // F.bGroup
    std::uint8_t  klass     = 0;   // C.bClass
    std::uint8_t  level     = 0;   // C.bLevel
};

// Reverse edge: a char who added this char (legacy CTBLFriendTarget —
// id + name only; level/class/group stay 0).
struct FriendReverseRow
{
    std::uint32_t char_id = 0;     // F.dwCharID
    std::string   name;            // C.szName
};

} // namespace tworldsvr

namespace fourstory::db::orm {

template<>
struct EntityMapping<tworldsvr::FriendForwardRow>
{
    using T = tworldsvr::FriendForwardRow;

    static constexpr const char* Table    = "TFRIENDTABLE";
    static constexpr const char* PkColumn = "dwFriendID";

    static T FromRow(const soci::row& r)
    {
        T row;
        row.friend_id = static_cast<std::uint32_t>(r.get<int>("dwFriendID"));
        row.name      = r.get<std::string>("szName");
        row.group     = static_cast<std::uint8_t>(r.get<int>("bGroup"));
        row.klass     = static_cast<std::uint8_t>(r.get<int>("bClass"));
        row.level     = static_cast<std::uint8_t>(r.get<int>("bLevel"));
        return row;
    }

    // Read-only via QueryBound (caller passes the full JOIN SQL); the
    // generic CRUD strings exist only to satisfy the mapping surface.
    static std::string SelectAllSql()  { return ""; }
    static std::string SelectByIdSql() { return ""; }
    static std::string DeleteSql()     { return ""; }
    static std::string InsertSql()     { return ""; }
    static std::string UpdateSql()     { return ""; }
    static void BindInsert(soci::statement&, const T&) {}
    static void BindUpdate(soci::statement&, const T&) {}
    static int  GetPk(const T& r) { return static_cast<int>(r.friend_id); }
};

template<>
struct EntityMapping<tworldsvr::FriendReverseRow>
{
    using T = tworldsvr::FriendReverseRow;

    static constexpr const char* Table    = "TFRIENDTABLE";
    static constexpr const char* PkColumn = "dwCharID";

    static T FromRow(const soci::row& r)
    {
        T row;
        row.char_id = static_cast<std::uint32_t>(r.get<int>("dwCharID"));
        row.name    = r.get<std::string>("szName");
        return row;
    }

    static std::string SelectAllSql()  { return ""; }
    static std::string SelectByIdSql() { return ""; }
    static std::string DeleteSql()     { return ""; }
    static std::string InsertSql()     { return ""; }
    static std::string UpdateSql()     { return ""; }
    static void BindInsert(soci::statement&, const T&) {}
    static void BindUpdate(soci::statement&, const T&) {}
    static int  GetPk(const T& r) { return static_cast<int>(r.char_id); }
};

} // namespace fourstory::db::orm
