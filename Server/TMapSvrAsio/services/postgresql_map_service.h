#pragma once
#include "player_service.h"
#include "session_validator.h"
#include "server_route_resolver.h"
#include "fourstory/db/session_pool.h"
#include <string>

namespace tmapsvr {
namespace transfer {struct State;struct Item;}
struct PostgreSQLMapConfig {
    std::uint8_t world{},server{};
    std::string owner_token,character_manifest,routing_manifest,actor_manifest;
};
class PostgreSQLMapService final : public IPlayerService, public IMapSessionValidator, public IServerRouteResolver {
public:
    PostgreSQLMapService(fourstory::db::SessionPool& pool,PostgreSQLMapConfig config);
    std::optional<MapSessionInfo> LookupSession(std::uint32_t user,std::uint32_t key) override;
    std::optional<MapSessionInfo> ClaimSession(const MapSessionClaim&,const MapSessionInfo&) override;
    std::optional<std::vector<std::uint8_t>> NeighborServers(const MapSessionClaim&,std::uint16_t,float,float) override;
    std::optional<std::vector<std::uint8_t>> MovementServers(const MapSessionClaim&,std::uint16_t,float,float) override;
    bool OwnsCell(const MapSessionClaim&,std::uint16_t,float,float) override;
    bool AuthorizeReplicas(const MapSessionClaim&,std::uint16_t,float,float,const std::vector<ServerRoute>&) override;
    bool LoadReplica(const MapSessionClaim&,std::uint16_t,float,float) override;
    std::vector<ServerRoute> Resolve(std::uint8_t,const std::vector<std::uint8_t>&) override;
    std::vector<ServerRoute> ResolveAuthorized(const MapSessionClaim&,const std::vector<std::uint8_t>&) override;
    void MarkReady(const MapSessionClaim&) override;
    void MarkReady(const MapSessionClaim&,const CharSnapshot&) override;
    void CheckpointAuthorized(const MapSessionClaim&,const CharSnapshot&,std::uint64_t revision);
    void ReleaseSession(const MapSessionClaim&) override;
    std::optional<CharSnapshot> LoadChar(std::uint32_t) override;
    void SaveChar(const CharSnapshot&) override;
    std::optional<CharSnapshot> LoadAuthorized(const MapSessionClaim&) override;
    void SaveAuthorized(const MapSessionClaim&,const CharSnapshot&) override;
    bool PrepareTransfer(const MapSessionClaim&,const CharSnapshot&,std::span<const std::byte>) override;
    std::optional<TransferredCharacter> AcceptTransfer(const MapSessionClaim&,std::span<const std::byte>) override;
    std::optional<TransferredCharacter> ConfirmAcceptedTransfer(const MapSessionClaim&,std::span<const std::byte>) override;
    bool OutgoingTransferCommitted(const MapSessionClaim&) override;
    std::vector<std::string> ConsumeSkillItems(const MapSessionClaim&,std::uint16_t,std::uint8_t,const std::vector<SkillItemDebit>&,const CharSnapshot&) override;
    SkillCastCommit CommitSkillCast(const MapSessionClaim&,const SkillCastRequest&,const CharSnapshot&,const CharSnapshot&) override;
    SecureCommit UpdateSecureCode(const MapSessionClaim&,std::uint16_t,std::span<const std::byte>,const CharSnapshot&) override;
    bool ConfirmReplicaLevel(const MapSessionClaim&,std::uint8_t) override;
    QuestCommit ChangeQuest(const MapSessionClaim&,std::uint16_t,std::span<const std::byte>,const CharSnapshot&,const IQuestChart&,const QuestMapFacts&) override;
    TitleChangeCommit SelectTitle(const MapSessionClaim&,std::span<const std::byte>,const CharSnapshot&) override;
    SkillPurchaseCommit PurchaseSkill(const MapSessionClaim&,std::span<const std::byte>,const CharSnapshot&) override;
    SkillActionCommit CommitSkillAction(const MapSessionClaim&,std::span<const std::byte>,const CharSnapshot&) override;
    FinishSkillCommit FinishSkill(const MapSessionClaim&,std::span<const std::byte>,const CharSnapshot&) override;
    InventoryMoveCommit MoveInventoryItems(const MapSessionClaim&,const InventoryMoveRequest&,const CharSnapshot&,const CharSnapshot&) override;
    EffectEndCommit EndMaintainedEffect(const MapSessionClaim&,const EffectEndRequest&,const CharSnapshot&) override;
private:
    std::optional<TransferredCharacter> AcceptTransferOrConfirm(const MapSessionClaim&,std::span<const std::byte>,bool confirmation_only);
    static std::vector<QuestProgressRow> ReadQuestProgress(soci::session&,const MapSessionClaim&);
    static void HydrateQuestProgress(soci::session&,const MapSessionClaim&,CharacterPayload&);
    static void CheckQuestProgress(soci::session&,const MapSessionClaim&,const CharSnapshot&);
    static void WriteQuestProgress(soci::session&,const MapSessionClaim&,const CharSnapshot&);
    static void WriteQuestTimers(soci::session&,const MapSessionClaim&,const CharSnapshot&);
    static SecureCodeState ReadSecurity(soci::session&,const MapSessionClaim&);
    static void CheckCastHead(soci::session&,const MapSessionClaim&,const CharSnapshot&);
    std::vector<std::string> CommitSkillMutation(const MapSessionClaim&,std::uint16_t,std::uint8_t,const std::vector<SkillItemDebit>&,const CharSnapshot&,const SkillCastRequest*,const CharSnapshot*,SkillCastCommit*);
    CharSnapshot ValidateSkillCast(soci::session&,const MapSessionClaim&,const SkillCastRequest&,const CharSnapshot&,const CharSnapshot&) const;
    struct InventoryStoragePlan {InventoryMovePlan move;CharSnapshot after;InventoryMoveCommit committed;std::string before_graph,after_graph;};
    InventoryStoragePlan ValidateInventoryMove(soci::session&,const MapSessionClaim&,const InventoryMoveRequest&,const CharSnapshot&,const CharSnapshot&) const;
    static void RefreshEquipment(soci::session&,CharSnapshot&,bool derive);
    static ItemInstance ProjectNativeItem(soci::session&,const transfer::Item&);
    static std::optional<ItemInstance> CreateQuestItem(soci::session&,const MapSessionClaim&,std::uint16_t,std::uint8_t);
    static std::vector<ItemInstance> CreateQuestRewardItems(soci::session&,const MapSessionClaim&,const CharSnapshot&,const QuestRewardDef&);
    static bool GrantQuestItems(soci::session&,const MapSessionClaim&,CharSnapshot&,const std::vector<ItemInstance>&,QuestPackets&);
    static bool OpenSpecialBox(soci::session&,const MapSessionClaim&,CharSnapshot&,std::uint16_t,QuestPackets&);
    static void ConsumeQuestItems(soci::session&,const MapSessionClaim&,CharSnapshot&,const QuestDef&,QuestPackets&);
    using QuestEvent = std::function<void(std::uint32_t,std::uint8_t,std::uint8_t,int)>;
    static void AdvanceProgressEffects(soci::session&,const MapSessionClaim&,CharSnapshot&,std::uint64_t,const QuestEvent&,QuestPackets&);
    static void HydrateDuringItems(soci::session&,const MapSessionClaim&,CharacterPayload&);
    static void CheckDuringItems(soci::session&,const MapSessionClaim&,const CharSnapshot&);
    static void WriteDuringItems(soci::session&,const MapSessionClaim&,CharSnapshot&);
    static void EnterDuringItems(soci::session&,const MapSessionClaim&,CharSnapshot&,QuestPackets&);
    static void AdvanceDuringItems(soci::session&,const MapSessionClaim&,CharSnapshot&,QuestPackets&);
    static void HangExpBenefit(soci::session&,const MapSessionClaim&,CharSnapshot&,QuestPackets&);
    static void ForceProgressEffect(soci::session&,const MapSessionClaim&,CharSnapshot&,std::uint16_t,std::uint32_t,QuestPackets&);
    static void EraseProgressEffect(soci::session&,const MapSessionClaim&,CharSnapshot&,std::size_t,QuestPackets&);
    static bool UseNativeItem(soci::session&,const MapSessionClaim&,CharSnapshot&,std::span<const std::byte>,const QuestEvent&,QuestPackets&);
    static void EarnNativeMoney(soci::session&,const MapSessionClaim&,CharSnapshot&,std::uint32_t,QuestPackets&);
    static bool UseNativeMoney(soci::session&,const MapSessionClaim&,CharSnapshot&,std::uint32_t,bool,QuestPackets&);
    static bool ChangeSoulLottery(soci::session&,const MapSessionClaim&,CharSnapshot&,bool,QuestPackets&);
    static void DebitNativeItem(soci::session&,const MapSessionClaim&,CharSnapshot&,std::uint8_t,std::uint8_t,std::uint64_t,std::uint8_t,QuestPackets&);
    static bool SellNativeItem(soci::session&,const MapSessionClaim&,CharSnapshot&,std::span<const std::byte>,std::uint16_t&,QuestPackets&);
    static bool RepairNativeItems(soci::session&,const MapSessionClaim&,CharSnapshot&,std::span<const std::byte>,QuestPackets&);
    static std::vector<ItemCooldown> ReadItemCooldowns(soci::session&,const MapSessionClaim&);
    static void HydrateItemCooldowns(soci::session&,const MapSessionClaim&,const CharSnapshot&,CharacterPayload&);
    static void CheckItemCooldowns(soci::session&,const MapSessionClaim&,const CharSnapshot&);
    static void WriteItemCooldowns(soci::session&,const MapSessionClaim&,const CharSnapshot&);
    static bool ItemCooldownCheckpointMatches(soci::session&,const MapSessionClaim&,const CharSnapshot&);
    static std::uint8_t CompleteQuestRewards(soci::session&,const MapSessionClaim&,CharSnapshot&,const QuestDef&,
        std::uint8_t,std::uint32_t,const std::function<std::uint8_t()>&,const QuestEvent&,QuestPackets&);
    static void GainQuestExperience(soci::session&,const MapSessionClaim&,CharSnapshot&,std::uint32_t,const QuestEvent&,QuestPackets&);
    std::string ValidateInventoryState(soci::session&,const MapSessionClaim&,const CharSnapshot&,bool all_fresh_items) const;
    enum class ConsumptionKind { Reagent, Ammunition, MultiAttackAmmunition };
    struct ReagentGraphPlan {std::string before_hash,after_hash;std::vector<std::string> item_hashes;ConsumptionKind kind=ConsumptionKind::Reagent;};
    static std::string GraphItemFingerprint(const transfer::Item&);
    ReagentGraphPlan ValidateGraphReagent(soci::session&,const MapSessionClaim&,std::uint16_t,std::uint8_t,const std::vector<SkillItemDebit>&,const CharSnapshot&) const;
    static ConsumptionKind ValidateSkillConsumption(soci::session&,const MapSessionClaim&,std::uint16_t,std::uint8_t,const std::vector<SkillItemDebit>&,const CharSnapshot&,const transfer::State*);
    friend int RecoverPreparedMapTransfers(soci::session&,int,int,const std::string&);
    static void StoreSkillCheckpoint(soci::session&,const MapSessionClaim&,const CharSnapshot&);
    static bool SkillCheckpointMatches(soci::session&,const MapSessionClaim&,const CharSnapshot&);
    static bool MaintainCheckpointMatches(soci::session&,const MapSessionClaim&,const CharSnapshot&);
    static void WriteMaintainedEffects(soci::session&,const MapSessionClaim&,const CharSnapshot&);
    void StoreTransferCheckpoint(soci::session&,const MapSessionClaim&,const CharSnapshot&) const;
    std::string TransferFingerprint(const MapSessionClaim&,const CharSnapshot&) const;
    std::optional<CharSnapshot> RestoreTransferCheckpoint(soci::session&,const MapSessionClaim&) const;
    CharSnapshot HydrateTransfer(soci::session&,const transfer::State&,const MapSessionClaim&) const;
    int CellOwner(soci::session&,const MapSessionClaim&,std::uint16_t,float,float) const;
    std::string CoreFingerprint(const MapSessionClaim&,const CharSnapshot&) const;
    void CheckCatalogs(soci::session&) const;
    bool LockAccount(soci::session&,const MapSessionClaim&) const;
    bool LockClaim(soci::session&,const MapSessionClaim&,std::string& phase) const;
    std::optional<std::vector<std::uint8_t>> NeighborServersLocked(soci::session&,const MapSessionClaim&,std::uint16_t,float,float) const;
    std::optional<MapSessionInfo> ClaimReplica(soci::session&,const MapSessionClaim&) const;
    bool LockReplica(soci::session&,const MapSessionClaim&,std::string& phase) const;
    void MarkReplicaReady(const MapSessionClaim&);
    void ReleaseReplica(soci::session&,const MapSessionClaim&) const;
    void CloseClaim(soci::session&,const MapSessionClaim&) const;
    static void WriteCore(soci::session&,const MapSessionClaim&,const CharSnapshot&,int logout);
    void RecordCheckpoint(soci::session&,const MapSessionClaim&,long long revision,const std::string& fingerprint,const std::string& outcome) const;
    fourstory::db::SessionPool& m_pool;
    PostgreSQLMapConfig m_config;
};
}
