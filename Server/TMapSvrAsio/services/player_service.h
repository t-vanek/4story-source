#pragma once

// Player service — loads a character snapshot from TCHARTABLE.
//
// The CharSnapshot struct holds the fields the F8 DM_LOADCHAR_ACK
// success branch encodes onto the wire. Later phases extend the
// snapshot (and the encoded body) with items (F9), skills (F11),
// quests (F12), …; each phase fills in the section it owns. The
// trailing sub-sections of legacy DM_LOADCHAR_ACK (secure code, aid
// table, PC bang, post info, inventory, cabinet, …) are emitted with
// default / empty values in F8 and replaced as the responsible
// service comes online.

#include "domain/character.h"
#include "domain/session.h"
#include "domain/skill_cast.h"
#include "services/finish_skill.h"
#include "services/skill_action.h"
#include "services/skill_learning.h"
#include "services/secure_code.h"
#include "services/quest_runtime.h"
#include "domain/party.h"
#include "native_map_cells.h"

#include <cstdint>
#include <optional>
#include <span>
#include <cstddef>
#include <stdexcept>

namespace tmapsvr {

struct TransferredCharacter {CharSnapshot snapshot;std::uint64_t authority_epoch{};};

class IPlayerService
{
public:
    virtual ~IPlayerService() = default;
    virtual void CheckControlOwner() {throw std::runtime_error("Native Control ownership unavailable");}
    virtual bool AuthorizeControlKick(const MapSessionClaim&,const std::string&) {throw std::runtime_error("Native Control kick authority unavailable");}
    // Immutable adapter capability, never a fallback after failed native proof.
    virtual bool HasNativePartyAuthority() const {return false;}
    virtual bool HasNativeCellAuthority() const {return false;}
    // Transient source EnterMAP/OnMove/LeaveMAP. Called on the game executor,
    // after durable admission and before actual client event publication.
    virtual NativeCellNotices EnterNativePlayer(const MapSessionClaim&,const CharSnapshot&) {
        throw std::runtime_error("Native player cells unavailable");
    }
    virtual NativeCellNotices MoveNativePlayer(const MapSessionClaim&,std::uint16_t,float,float,bool=true) {
        throw std::runtime_error("Native player cells unavailable");
    }
    virtual NativeCellNotices LeaveNativePlayer(const MapSessionClaim&) {
        throw std::runtime_error("Native player cells unavailable");
    }
    virtual NativeCellNotices RebindNativePlayer(const MapSessionClaim&,const MapSessionClaim&,std::uint16_t,float,float) {
        throw std::runtime_error("Native player cell handoff unavailable");
    }
    virtual std::optional<PartyAuthority> AuthorizeParty(const MapSessionClaim&,const CharSnapshot&,
        std::uint32_t=0,bool=false) {throw std::runtime_error("Native party actor authority unavailable");}
    virtual std::optional<CommunicationAuthority> AuthorizeCommunication(const MapSessionClaim&,bool=false,std::uint32_t=0,std::uint8_t=0) {
        throw std::runtime_error("Native communication authority unavailable");
    }
    virtual bool ConfirmChatBan(const MapSessionClaim&,const std::string&,std::int64_t,bool=false) {
        throw std::runtime_error("Native chat ban authority unavailable");
    }
    virtual std::optional<std::string> NoticeOperatorName(const MapSessionClaim&) {
        throw std::runtime_error("Native notice text authority unavailable");
    }
    virtual bool ConfirmReplicaLevel(const MapSessionClaim&,std::uint8_t) {throw std::runtime_error("Replica level authority unavailable");}
    virtual bool ConfirmSoulmateState(const MapSessionClaim&,std::uint32_t,const std::string&,std::uint32_t) {
        throw std::runtime_error("Native soulmate state authority unavailable");
    }
    virtual QuestCommit ChangeQuest(const MapSessionClaim&,std::uint16_t,std::span<const std::byte>,const CharSnapshot&,const IQuestChart&,const QuestMapFacts&) {throw std::runtime_error("Native quest persistence unavailable");}
    virtual void UpdateRpsChart(std::span<const std::byte>) {throw std::runtime_error("Native RPS catalog unavailable");}
    virtual std::vector<std::byte> BattleHeroList(const MapSessionClaim&) {throw std::runtime_error("Native battle-region state unavailable");}
    virtual void RetireBattleRegions() {throw std::runtime_error("Native battle-region retirement unavailable");}
    virtual bool BattleRegionsCurrent() const {return false;}
    virtual SecureCommit UpdateSecureCode(const MapSessionClaim&,std::uint16_t,std::span<const std::byte>,const CharSnapshot&) {throw std::runtime_error("Secure code persistence unavailable");}

    // Load the snapshot for `char_id`, or nullopt when the row is
    // missing / the DB is in trouble. The handler distinguishes
    // "no row" (CN_NOCHAR in legacy) from "lookup error" by checking
    // the optional and the configured pool's health independently.
    virtual std::optional<CharSnapshot>
        LoadChar(std::uint32_t char_id) = 0;

    // Persist the live snapshot back to TCHARTABLE + TALLCHARTABLE.
    // Called by the MapServer teardown hook on every authenticated
    // disconnect. Mirrors the combined effect of TMapSvr's char-save
    // path and legacy TLogout's TALLCHARTABLE update.
    virtual void SaveChar(const CharSnapshot& snap) = 0;
    virtual std::optional<CharSnapshot> LoadAuthorized(const MapSessionClaim& claim) {
        return LoadChar(claim.char_id);
    }
    virtual void SaveAuthorized(const MapSessionClaim&, const CharSnapshot& snap) {
        if(snap.persistence_uncertain)throw std::runtime_error("Uncertain durable gameplay outcome");
        SaveChar(snap);
    }
    virtual bool PrepareTransfer(const MapSessionClaim&,const CharSnapshot&,std::span<const std::byte>) {return false;}
    virtual std::optional<TransferredCharacter> AcceptTransfer(const MapSessionClaim&,std::span<const std::byte>) {return {};}
    // After an unknown commit outcome, only confirm an existing exact receipt.
    // This operation must never promote a still-prepared transfer.
    virtual std::optional<TransferredCharacter> ConfirmAcceptedTransfer(const MapSessionClaim&,std::span<const std::byte>) {
        throw std::runtime_error("Native transfer confirmation unavailable");
    }
    virtual bool OutgoingTransferCommitted(const MapSessionClaim&) {return false;}
    virtual std::vector<std::string> ConsumeSkillItems(const MapSessionClaim&,std::uint16_t,std::uint8_t,
        const std::vector<SkillItemDebit>&,const CharSnapshot&) {throw std::runtime_error("Native skill item transaction unavailable");}
    virtual SkillCastCommit CommitSkillCast(const MapSessionClaim&,const SkillCastRequest&,const CharSnapshot&,const CharSnapshot&) {throw std::runtime_error("Native cast transaction unavailable");}
    virtual TitleChangeCommit SelectTitle(const MapSessionClaim&,std::span<const std::byte>,const CharSnapshot&) {throw std::runtime_error("Native title selection unavailable");}
    virtual SkillPurchaseCommit PurchaseSkill(const MapSessionClaim&,std::span<const std::byte>,const CharSnapshot&) {throw std::runtime_error("Native skill learning unavailable");}
    virtual SkillActionCommit CommitSkillAction(const MapSessionClaim&,std::span<const std::byte>,const CharSnapshot&) {throw std::runtime_error("Native skill action unavailable");}
    virtual FinishSkillCommit FinishSkill(const MapSessionClaim&,std::span<const std::byte>,const CharSnapshot&) {throw std::runtime_error("Native skill finish unavailable");}
    virtual InventoryMoveCommit MoveInventoryItems(const MapSessionClaim&,const InventoryMoveRequest&,
        const CharSnapshot&,const CharSnapshot&) {throw std::runtime_error("Native inventory move transaction unavailable");}
    virtual EffectEndCommit EndMaintainedEffect(const MapSessionClaim&,const EffectEndRequest&,
        const CharSnapshot&) {throw std::runtime_error("Native effect cancellation unavailable");}
    std::string ConsumeSkillItem(const MapSessionClaim& c,std::uint16_t skill,
        const ItemInstance& before,const CharSnapshot& after) {
        return ConsumeSkillItems(c,skill,1,{{before,1}},after).at(0);
    }

};

} // namespace tmapsvr
