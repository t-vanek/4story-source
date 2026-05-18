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
    // Equipped items omitted in Phase A — wire-format slot is
    // serialized as bEquipCount=0 + empty list. Phase B will populate.
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
};

} // namespace tloginsvr::services
