#pragma once
#include "native_monster_spawn.h"
#include <cstddef>
#include <tuple>

namespace tmapsvr {
struct NativeAiCondition {std::uint32_t id{};std::uint8_t kind{};};
struct NativeAiCommand {
    std::uint32_t id{};
    std::uint8_t kind{};
    std::vector<NativeAiCondition> conditions;
};
struct NativeAiBinding {
    std::uint64_t source_row{}; // stable identity, never a legacy pointer handle
    std::uint32_t command{},delay{};
    std::uint8_t loop{};
};
struct NativeMonsterAiCatalog {
    std::map<std::uint32_t,NativeAiCommand> commands;
    std::map<std::tuple<std::uint8_t,std::uint8_t,std::uint32_t>,std::vector<NativeAiBinding>> bindings;
    std::set<std::uint8_t> types;
    std::map<std::uint64_t,NativeAiBinding> by_source_row; // immutable command-instance identity index
    std::string order_sha256;
};
enum class NativeAiMessageKind { Command, ResetHost };
struct NativeMonsterHostCountry {std::uint8_t country{},war_country{};};
struct NativeAiInvocation {
    std::uint64_t binding{},generation{};
    std::uint32_t monster{},host_key{},event_host{},real_host{},delay{};
    std::uint16_t map{},party{};
    std::uint8_t channel{},real_type{};
    NativeAiMessageKind kind{NativeAiMessageKind::Command};
};

// The existing bootstrap executor extended to delayed ACKs. Still restricted
// to a PROVEN actor-free population. LoadData supplies tick0; runtime supplies
// the AI thread's current DWORD clock. Actor admission must enforce this scope.
class NativeMonsterAiExecutor {
public:
    // Publishes cells/visibility only; Event(AT_ENTER) is emitted here once
    // after this consumer succeeds. It must not emit a duplicate AI event.
    using EnterMap=std::function<void(NativeMonsterMap&,std::uint32_t)>;
    using LeaveMap=std::function<void(NativeMonsterMap&,std::uint32_t,bool)>;
    using Publish=std::function<void(NativeCellNotices)>;
    using FindPlayer=std::function<std::optional<MapSessionClaim>(std::uint32_t)>;
    // Reads actual current source GetCountry/GetWarCountry inputs, not cached
    // cell-country defaults. Unsupported disguise state must refuse.
    using ReadPlayerCountry=std::function<NativeMonsterHostCountry(const MapSessionClaim&)>;
    using MoveMap=std::function<bool(NativeMonsterMap&,std::uint32_t,float,float)>;
    using Schedule=std::function<void(const NativeAiInvocation&)>;
    using Cancel=std::function<void(const NativeAiInvocation&)>;
    using Draw=std::function<std::uint32_t()>;
    NativeMonsterAiExecutor(const NativeMonsterCatalog&,const NativeMonsterAiCatalog&,
                           const BattleRegions&,EnterMap,LeaveMap,Publish,FindPlayer,ReadPlayerCountry,MoveMap,Schedule,Cancel,Draw,std::uint32_t tick);
    void Initialize(NativeMonsterMap&,std::uint32_t);
    void Acknowledge(NativeMonsterMap&,const NativeAiInvocation&);
    void ResetHostAcknowledgement(NativeMonsterMap&,const NativeAiInvocation&);
private:
    struct Arguments {std::uint32_t event_host{},real_host{};std::uint8_t real_type{};};
    const std::vector<NativeAiBinding>* Bindings(const NativeMonsterSlot&,std::uint8_t,std::uint32_t) const;
    bool Conditions(const NativeAiCommand&,const NativeMonsterSlot&);
    bool CanRun(const NativeMonsterMap&,const NativeAiCommand&,const NativeMonsterSlot&,Arguments);
    std::uint32_t Delay(const NativeAiBinding&,const NativeMonsterSlot&);
    bool Execute(NativeMonsterMap&,std::uint32_t,const NativeAiBinding&,Arguments);
    bool Complete(NativeMonsterMap&,std::uint32_t,const NativeAiBinding&,Arguments);
    bool Regen(NativeMonsterMap&,std::uint32_t,const NativeAiBinding&,Arguments);
    void Event(NativeMonsterMap&,std::uint32_t,std::uint8_t,std::uint32_t,Arguments);
    void DoCommand(NativeMonsterMap&,std::uint32_t,const NativeAiBinding&,Arguments);
    void ResetHost(NativeMonsterMap&,std::uint32_t);
    void ChangeMode(NativeMonsterMap&,std::uint32_t,std::uint8_t);
    bool LeaveAggro(NativeMonsterMap&,std::uint32_t,Arguments);
    void SetEventToFollower(NativeMonsterMap&,std::uint32_t);
    bool ChangeHost(NativeMonsterMap&,std::uint32_t,const NativeAiBinding&,Arguments);
    bool Attack(NativeMonsterMap&,std::uint32_t,const NativeAiBinding&,Arguments,bool begin);
    bool CheckAttack(const NativeMonsterSlot&) const;
    void ResolveTemplate(NativeMonsterSlot&,const NativeMonsterAllocationTemplate&);
    std::uint32_t Random();
    void Step();
    const NativeMonsterCatalog& m_catalog;
    const NativeMonsterAiCatalog& m_ai;
    const BattleRegions& m_regions;
    EnterMap m_enter;
    LeaveMap m_leave;
    Publish m_publish;
    FindPlayer m_find_player;
    ReadPlayerCountry m_player_country;
    MoveMap m_move;
    Schedule m_schedule;
    Cancel m_cancel;
    Draw m_draw;
    std::uint32_t m_tick;
    std::size_t m_steps{},m_depth{};
};
} // namespace tmapsvr
