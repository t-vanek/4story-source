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
    bool OutgoingTransferCommitted(const MapSessionClaim&) override;
    std::vector<std::string> ConsumeSkillItems(const MapSessionClaim&,std::uint16_t,std::uint8_t,const std::vector<SkillItemDebit>&,const CharSnapshot&) override;
    InventoryMoveCommit MoveInventoryItems(const MapSessionClaim&,const InventoryMoveRequest&,const CharSnapshot&,const CharSnapshot&) override;
    EffectEndCommit EndMaintainedEffect(const MapSessionClaim&,const EffectEndRequest&,const CharSnapshot&) override;
private:
    struct InventoryStoragePlan {InventoryMovePlan move;CharSnapshot after;InventoryMoveCommit committed;std::string before_graph,after_graph;};
    InventoryStoragePlan ValidateInventoryMove(soci::session&,const MapSessionClaim&,const InventoryMoveRequest&,const CharSnapshot&,const CharSnapshot&) const;
    static void RefreshEquipment(soci::session&,CharSnapshot&,bool derive);
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
    CharSnapshot HydrateTransfer(soci::session&,const transfer::State&) const;
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
