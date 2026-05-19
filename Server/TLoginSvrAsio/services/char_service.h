#pragma once

// ICharService — character lifecycle (list / create / delete) for
// the lobby flow. Backs CS_CHARLIST_REQ, CS_CREATECHAR_REQ, and
// CS_DELCHAR_REQ.
//
// Schema mapping (legacy compat):
//   * List   → SELECT … FROM TCHARTABLE WHERE dwUserID = ? AND bWorldID = ?
//              joined with TITEMTABLE for equipped items (legacy CTBLItem)
//   * Create → INSERT into TCHARTABLE + TALLCHARTABLE + starter inventory
//              (TCreateChar SP at Server/TLoginSvr-side DB)
//   * Delete → soft-delete (bDelete=1) if level > 5; hard delete otherwise.
//              Guild membership blocks delete (TDeleteChar SP)
//
// Password verification for CS_DELCHAR_REQ is split off into the
// auth service in the real impl (Phase B); the in-memory stub
// accepts the password parameter but doesn't validate it.

#include <cstdint>
#include <string>
#include <vector>

namespace tloginsvr::services {

// One equipped item slot in CS_CHARLIST_ACK. Wire layout per item
// matches what the shipped client parses in TNetHandler.cpp:425-440:
//   bItemID, wItemID, bLevel, bGradeEffect, wColor, wCustomTex,
//   bRegGuild, wMoggItemID  (12 bytes total).
//
// An earlier revision dropped `wCustomTex` because legacy server's
// DEF_UDPLOG branch (CSHandler.cpp:749-756) doesn't emit it. The
// shipped client was built against the non-DEF_UDPLOG branch
// (CSHandler.cpp:935-943) and reads `wCustomTex` between `wColor`
// and `bRegGuild`. Without the field the client parses every
// following byte at the wrong offset, rendering characters with
// garbled equipment.
//
// The legacy CTBLItem query aliases dwColor → dwTime3 and dwRegGuild
// → dwTime4 because the shipped TITEMTABLE schema repurposed those
// time columns (Server/TLoginSvr/DBAccess.h:607-628). wCustomTex
// stays in its own column.
struct EquipItem
{
    std::uint8_t  item_id       = 0;   // bItemID — slot category (weapon/body/…)
    std::uint16_t item_kind     = 0;   // wItemID — item template id
    std::uint8_t  level         = 0;
    std::uint8_t  grade_effect  = 0;
    std::uint16_t color         = 0;   // dwTime3 → wColor
    std::uint16_t custom_tex    = 0;   // wCustomTex — custom texture skin
    std::uint8_t  reg_guild     = 0;   // dwTime4 → bRegGuild
    std::uint16_t mogg_item_id  = 0;
};

// One row in CS_CHARLIST_ACK. Field names match the legacy wire
// layout from CSHandler.cpp::OnCS_CHARLIST_REQ.
struct CharacterInfo
{
    std::int32_t  char_id      = 0;
    std::string   name;
    std::uint8_t  start_act    = 0;
    std::uint8_t  slot         = 0;
    std::uint8_t  level        = 1;
    std::uint8_t  char_class   = 0;  // `class` is a keyword
    std::uint8_t  race         = 0;
    std::uint8_t  country      = 0;
    std::uint8_t  sex          = 0;
    std::uint8_t  hair         = 0;
    std::uint8_t  face         = 0;
    std::uint8_t  body         = 0;
    std::uint8_t  pants        = 0;
    std::uint8_t  hand         = 0;
    std::uint8_t  foot         = 0;
    std::uint32_t region       = 0;
    std::uint32_t fame         = 0;
    std::uint32_t fame_color   = 0;
    std::uint8_t  helmet_hide  = 0;
    // Equipped items + guild fame. Populated by the SOCI backend
    // (TITEMTABLE/TGUILDMEMBERTABLE/TGUILDTABLE JOINs); the in-memory
    // backend leaves both at default. Wire-format encoder sees an
    // empty vector and emits bEquipCount=0.
    std::vector<EquipItem> items;
};

// CS_CREATECHAR_REQ payload, parsed.
struct CharacterCreateRequest
{
    std::int32_t  user_id = 0;
    std::uint8_t  group_id = 0;
    std::string   name;
    std::uint8_t  slot = 0;
    std::uint8_t  char_class = 0;
    std::uint8_t  race = 0;
    std::uint8_t  country = 0;
    std::uint8_t  sex = 0;
    std::uint8_t  hair = 0;
    std::uint8_t  face = 0;
    std::uint8_t  body = 0;
    std::uint8_t  pants = 0;
    std::uint8_t  hand = 0;
    std::uint8_t  foot = 0;
    std::uint8_t  level_option = 0;  // veteran-bonus selector
};

// Legacy CR_* result codes from NetCode.h.
enum class CreateCharResult : std::uint8_t
{
    Success       = 0,
    NoGroup       = 1,
    DuplicateName = 2,
    InvalidSlot   = 3,
    Protected     = 4,  // name charset / reserved
    OverChar      = 5,  // length out of range / slot count exhausted
    NeedCard      = 6,
    Internal      = 7,
};

struct CharacterCreateResponse
{
    CreateCharResult status = CreateCharResult::Internal;
    std::int32_t     char_id = 0;
    std::uint8_t     remaining_slots = 0;
    std::uint8_t     starting_level = 1;
};

// Legacy DR_* result codes.
enum class DeleteCharResult : std::uint8_t
{
    Success         = 0,
    Failed          = 1,  // in-guild
    InvalidPassword = 2,
    Internal        = 3,
    NoGroup         = 4,
};

// Three veteran-bonus level entries for CS_VETERAN_ACK. The wire
// format hardcodes 3 slots (CSSender.cpp:145-159). Empty cache → all
// zeros. If TVETERANCHART has more than 3 rows, only the lowest-bID
// three are returned.
struct VeteranLevels
{
    std::uint8_t first  = 0;
    std::uint8_t second = 0;
    std::uint8_t third  = 0;
};

class ICharService
{
public:
    virtual ~ICharService() = default;

    // CS_CHARLIST_REQ — list characters owned by user_id in group_id.
    virtual std::vector<CharacterInfo>
    List(std::int32_t user_id, std::uint8_t group_id) = 0;

    // CS_CREATECHAR_REQ
    virtual CharacterCreateResponse
    Create(const CharacterCreateRequest& req) = 0;

    // CS_DELCHAR_REQ. `password` is the client-supplied delete
    // confirmation; the in-memory stub doesn't validate it (real
    // impl in Phase B routes through IAuthService::VerifyPassword
    // before performing the delete).
    virtual DeleteCharResult
    Delete(std::int32_t user_id,
           std::uint8_t group_id,
           std::int32_t char_id,
           const std::string& password) = 0;

    // CS_VETERAN_REQ — first three rows of TVETERANCHART, cached at
    // construction. Returned to the client so the create-char screen
    // can show the available level-boost options.
    virtual VeteranLevels GetVeteranLevels() const = 0;

    // BR/BOW shard membership lookups. Return the dwCharID of the
    // user's enrolled BR or BOW char, if any. Used by the lobby
    // CHARLIST flow to send CS_BOWPLAYERNOTIFY_ACK with the matching
    // slot so the client can highlight the shard char in the UI.
    //
    // Legacy CSHandler.cpp:617-635 queries BOW first; if BOW returns
    // 0 it falls through to BR. The handler should mirror that order
    // so the user's primary shard wins when they're enrolled in both.
    //
    // Returns 0 when the user has no enrolled char or the lookup
    // fails (table missing, DB error). Modern treats both tables as
    // optional — absence is silently downgraded to 0.
    virtual std::int32_t GetBrCharId(std::int32_t user_id) = 0;
    virtual std::int32_t GetBowCharId(std::int32_t user_id) = 0;
};

} // namespace tloginsvr::services
