// Source-derived CONNECT vectors + real TCP MapServer admission/teardown.
// The World and persistence interfaces are controlled test doubles; this is
// deliberately not a certificate for the original client or PostgreSQL play.
#include "map_server.h"
#include "handlers_world.h"
#include "domain/connect.h"
#include "domain/skill_data.h"
#include "domain/main_transfer.h"
#include "services/session_registry.h"
#include "services/skill_cooldown.h"
#include "services/skill_chart.h"
#include "services/skill_reagent.h"
#include "services/main_transfer_runtime.h"
#include "services/client_senders.h"
#include "services/session_validator.h"
#include "services/player_service.h"
#include "services/char_state_store.h"
#include "services/channel_presence.h"
#include "services/world_client.h"
#include "wire_codec.h"
#include "audit/audit_log.h"
#include "services/log_peer.h"
#include <cstring>
#include <algorithm>
#include "MessageId.h"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <thread>

namespace asio = boost::asio;
using asio::ip::tcp;
using namespace std::chrono_literals;
using tnetlib::protocol::MessageId;
using tmapsvr::wire::WritePOD;
using Bytes = std::vector<std::byte>;
int passed = 0;
Bytes transport_secret;
void Check(bool ok, const char* label) {
    if (!ok) throw std::runtime_error(label);
    ++passed; std::printf("PASS %s\n", label);
}
asio::awaitable<void> Pause(std::chrono::milliseconds ms) {
    asio::steady_timer timer(co_await asio::this_coro::executor);
    timer.expires_after(ms); co_await timer.async_wait(asio::use_awaitable);
}
template<class F> asio::awaitable<void> Until(F predicate, const char* label,
    std::chrono::milliseconds limit = 1500ms) {
    const auto deadline = std::chrono::steady_clock::now() + limit;
    while (!predicate() && std::chrono::steady_clock::now() < deadline) co_await Pause(2ms);
    Check(predicate(), label);
}
constexpr std::uint32_t kUser = 17, kChar = 42, kKey = 0xf2345679;
Bytes Connect(std::uint32_t character = kChar) {
    Bytes b;
    WritePOD<std::uint16_t>(b, 0x2918); WritePOD<std::uint8_t>(b, 2);
    WritePOD(b, kUser); WritePOD(b, character); WritePOD(b, kKey);
    WritePOD<std::uint32_t>(b, 0x0100007f); WritePOD<std::uint16_t>(b, 5815);
    // Fixed known answer for the valid request. Wrong-character case needs its
    // own valid checksum so it tests authorization instead of checksum rejection.
    WritePOD<std::uint64_t>(b, character == kChar ? 0x66d875d8acd02a12ULL :
        tmapsvr::ConnectChecksum(0x2918, kUser, character, kKey));
    return b;
}
Bytes Verdict(std::uint32_t key, std::uint8_t result = 0) {
    Bytes b; WritePOD(b, kChar); WritePOD(b, key); WritePOD(b, result);
    WritePOD<std::uint8_t>(b, 2); WritePOD<std::uint8_t>(b, 4); WritePOD<std::uint8_t>(b, 7);
    return b;
}
struct Validator final : tmapsvr::IMapSessionValidator {
    std::atomic<int> calls{0}; std::atomic<bool> slow{false};
    int claim_failure=0;
    std::atomic<int> replica_loads{0},replica_readies{0},replica_releases{0};
    std::atomic<int> primary_readies{0};
    tmapsvr::CharSnapshot ready_snapshot;
    tmapsvr::MapSessionInfo info;
    Validator() { info.dwUserID=kUser; info.dwKEY=kKey; info.dwCharID=kChar;
        info.bGroupID=3; info.bChannel=2; }
    std::optional<tmapsvr::MapSessionInfo> ClaimSession(const tmapsvr::MapSessionClaim&, const tmapsvr::MapSessionInfo& candidate) override {
        if (claim_failure==2) throw std::runtime_error("synthetic claim error");
        if (claim_failure==1) return std::nullopt;
        return candidate;
    }
    std::optional<tmapsvr::MapSessionInfo> LookupSession(std::uint32_t user, std::uint32_t key) override {
        ++calls; if (slow) std::this_thread::sleep_for(60ms);
        if (user != kUser || key != kKey) return std::nullopt;
        return info;
    }
    bool LoadReplica(const tmapsvr::MapSessionClaim& c,std::uint16_t map,float x,float z) override {
        ++replica_loads;return c.role==tmapsvr::MapSessionRole::Replica&&map==0&&x==4080&&z==3584;
    }
    void MarkReady(const tmapsvr::MapSessionClaim& c,const tmapsvr::CharSnapshot& s) override {
        if(c.role==tmapsvr::MapSessionRole::Replica)++replica_readies;
        else {ready_snapshot=s;++primary_readies;}
    }
    void ReleaseSession(const tmapsvr::MapSessionClaim& c) override {
        if(c.role==tmapsvr::MapSessionRole::Replica)++replica_releases;
    }
};
struct Players final : tmapsvr::IPlayerService {
    std::atomic<int> loads{0}, saves{0};
    std::atomic<bool> save_started{false}, hold_save{false}, fail_save{false};
    tmapsvr::CharSnapshot saved;
    bool native_payload=false,admission_timers=false;
    bool reagent_fixture=false;
    std::atomic<bool> hold_consumption{false},consumption_started{false},fail_consumption{false};
    std::atomic<int> consumptions{0};
    tmapsvr::CharSnapshot committed;
    std::optional<tmapsvr::CharSnapshot> LoadChar(std::uint32_t cid) override {
        ++loads; tmapsvr::CharSnapshot s; s.dwCharID=cid; s.szNAME="Admission";
        s.bLevel=1; s.dwHP=169; s.dwMP=163; s.wMapID=2010; s.fPosX=3664.405f;
        s.fPosY=86.16578f; s.fPosZ=557.2542f;
        if(native_payload) {
            auto p=std::make_shared<tmapsvr::CharacterPayload>();p->skills.push_back({7,3,0});
            tmapsvr::SkillTemplate t;t.wID=7;t.bUseMPType=1;t.dwUseMP=1000;
            t.bUseHPType=2;t.dwUseHP=10;t.f1stRateX=2.0f;t.bStartLevel=1;t.bNextLevel=1;
            t.dwReuseDelay=60000;t.nReuseDelayInc=250;t.dwLoopDelay=2000;t.items=tmapsvr::SkillItemGate::Allowed;t.bSpeedApply=1;t.bKind=1;t.dwKindDelay=4000;
            p->skill_templates.push_back(t);
            if(reagent_fixture) {
                p->skill_templates[0].items=tmapsvr::SkillItemGate::Reagent;p->skill_templates[0].wUseItem=8412;
                tmapsvr::ItemInstance item;item.dlID=123;item.wItemID=8412;item.bInvenID=255;item.bItemID=4;item.bCount=2;
                item.source=std::make_shared<tmapsvr::transfer::Item>();item.durable_hash=std::string(64,'a');
                p->bags.push_back({{255,3,0,0},{item}});
            }
            for(std::uint16_t id:{8,9}) {p->skills.push_back({id,1,0});tmapsvr::SkillTemplate other;other.wID=id;other.bKind=id==8?1:2;p->skill_templates.push_back(other);}
            if(admission_timers){p->skills[0].dwRemainTick=300000;p->skills[1].dwRemainTick=1;}
            p->skill_attack_timing=std::array<tmapsvr::SkillAttackTiming,3>{tmapsvr::SkillAttackTiming{500,80},{},{}};
            s.payload=p;s.dwMaxHP=169;s.dwMaxMP=163;
        }
        return s;
    }
    void SaveChar(const tmapsvr::CharSnapshot& s) override {
        save_started=true;
        const auto deadline=std::chrono::steady_clock::now()+1s;
        while (hold_save && std::chrono::steady_clock::now()<deadline) std::this_thread::sleep_for(2ms);
        if (fail_save) throw std::runtime_error("injected save failure");
        saved=s; ++saves;
    }
    std::vector<std::string> ConsumeSkillItems(const tmapsvr::MapSessionClaim&,std::uint16_t,std::uint8_t,
        const std::vector<tmapsvr::SkillItemDebit>& debits,const tmapsvr::CharSnapshot& s) override {
        consumption_started=true;
        const auto deadline=std::chrono::steady_clock::now()+1s;
        while(hold_consumption&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(2ms);
        if(fail_consumption)throw std::runtime_error("synthetic unknown item transaction outcome");
        committed=s;++consumptions;return std::vector<std::string>(debits.size(),std::string(64,'b'));
    }
};
struct World final : tmapsvr::IWorldClient {
    bool connected=true, registered=true, send_ok=true;
    std::vector<std::pair<std::uint16_t,Bytes>> packets;
    bool IsConnected() const override { return connected; }
    bool IsRegistered() const override { return connected && registered; }
    asio::awaitable<bool> SendPacket(std::uint16_t id, Bytes b) override {
        packets.emplace_back(id,std::move(b)); co_return send_ok;
    }
};
struct Client {
    std::shared_ptr<tnetlib::AsioSession> wire;
    std::vector<std::pair<std::uint16_t,Bytes>> packets;
    bool ended=false;
    std::size_t Count(MessageId id) const {
        std::size_t n=0; for (const auto& p:packets) n+=p.first==static_cast<std::uint16_t>(id); return n;
    }
};
// Source CHARINFO layout for this inventory-free fixture. Independent of the
// encoder's offsets; variable strings are consumed at their original boundaries.
std::vector<tmapsvr::SkillRow> AdmissionSkills(const Client& client) {
    auto it=std::find_if(client.packets.begin(),client.packets.end(),[](const auto& p){return p.first==static_cast<std::uint16_t>(MessageId::CS_CHARINFO_ACK);});
    if(it==client.packets.end())throw std::runtime_error("Missing CHARINFO");
    tmapsvr::wire::Reader r(it->second);
    auto skip=[&](unsigned n){std::uint8_t v;while(n--)if(!r.Read(v))throw std::runtime_error("Truncated CHARINFO");};
    auto string=[&]{std::string s;if(!r.ReadString(s))throw std::runtime_error("Invalid CHARINFO string");};
    skip(9);string();skip(30);string();skip(4);string();skip(86);
    std::uint8_t bags{},count{};
    if(!r.Read(bags)||bags||!r.Read(count))throw std::runtime_error("Unexpected admission inventory");
    std::vector<tmapsvr::SkillRow> skills(count);
    for(auto& s:skills)if(!r.Read(s.wSkillID)||!r.Read(s.bLevel)||!r.Read(s.dwRemainTick))throw std::runtime_error("Truncated skill list");
    return skills;
}
std::shared_ptr<Client> Dial(asio::io_context& io, std::uint16_t port) {
    auto client=std::make_shared<Client>(); tcp::socket socket(io);
    socket.connect({asio::ip::address_v4::loopback(),port});
    client->wire=std::make_shared<tnetlib::AsioSession>(std::move(socket),
        transport_secret.empty() ? tnetlib::PeerType::Server : tnetlib::PeerType::Client);
    if (!transport_secret.empty()) client->wire->EnableOutboundRC4(transport_secret);
    asio::co_spawn(io,[client]() -> asio::awaitable<void> {
        co_await client->wire->RunPackets([client](const tnetlib::DecodedPacket& packet) {
            client->packets.emplace_back(packet.wId,Bytes(packet.body.begin(),packet.body.end()));
        }); client->ended=true;
    },asio::detached);
    return client;
}
asio::awaitable<void> Send(std::shared_ptr<Client> c, MessageId id, Bytes b) {
    co_await c->wire->SendPacket(static_cast<std::uint16_t>(id),b);
}
struct LogPeer final : tmapsvr::ILogPeer {
    Bytes body;
    bool Enabled() const override { return true; }
    bool Send(std::span<const std::byte> data) override {body.assign(data.begin(),data.end());return true;}
};
void AuditRedaction() {
    LogPeer peer; tmapsvr::audit::AuditLog log(&peer);
    tmapsvr::audit::LoginAttemptEvent login{}; login.key=kKey; login.char_id=kChar;
    log.Emit(login);
    tmapsvr::audit::LoginAttemptEvent decoded{};
    Check(peer.body.size()==sizeof(decoded),"login audit retains existing binary layout");
    std::memcpy(&decoded,peer.body.data(),sizeof(decoded));
    Check(decoded.key==0 && decoded.char_id==kChar && login.key==kKey,"login audit sink redacts token without mutating caller");
    tmapsvr::audit::CharLoadEvent load{}; load.key=kKey; load.char_id=kChar;
    log.Emit(load);
    tmapsvr::audit::CharLoadEvent received{};
    Check(peer.body.size()==sizeof(received),"load audit retains existing binary layout");
    std::memcpy(&received,peer.body.data(),sizeof(received));
    Check(received.key==0 && received.char_id==kChar,"load audit sink redacts token before UDP transport");
}
void Vectors() {
    constexpr std::uint64_t expected[]{0x2918,0x336c3aebf71ab742,0x66d875d7ee353d6a,
        0x9a44b0c3e54fc350,0xcdb0ebafdc6a517c,0x011d269bd384df66,
        0x34896187ca9f654e,0x67f59c73c1b9eb74};
    for (std::uint32_t i=0;i<8;++i) Check(tmapsvr::ConnectChecksum(0x2918,1,1,i)==expected[i],"checksum mixing count known answer");
    Check(tmapsvr::ConnectChecksum(0x2918,0xffffffff,0xffffffff,0xffffffff)==0x336c3aecd71ab73cULL,"DWORD initial product and sum wrap before widening");
}
Bytes ReplicaComposite(std::uint32_t key=kKey) {
    Bytes b;WritePOD(b,kChar);WritePOD(b,key);WritePOD<std::uint8_t>(b,0);
    tmapsvr::wire::WriteString(b,"Replica");WritePOD<std::uint16_t>(b,0);
    WritePOD<float>(b,4080);WritePOD<float>(b,80);WritePOD<float>(b,3584);
    for(int i=0;i<3;++i)WritePOD<std::uint32_t>(b,0);
    tmapsvr::wire::WriteString(b,"");WritePOD<std::uint8_t>(b,0);WritePOD<std::uint8_t>(b,0);
    WritePOD<std::uint16_t>(b,0);WritePOD<std::uint8_t>(b,0);WritePOD<std::uint32_t>(b,0);
    tmapsvr::wire::WriteString(b,"");WritePOD<std::uint16_t>(b,0);WritePOD<std::uint8_t>(b,0);
    WritePOD<std::uint32_t>(b,0);WritePOD<std::uint16_t>(b,0);
    for(auto v:{1,0,4,3,0})WritePOD<std::uint8_t>(b,v);
    WritePOD<std::uint32_t>(b,0);WritePOD<std::int64_t>(b,0);WritePOD<std::uint32_t>(b,0);WritePOD<std::uint32_t>(b,0);
    tmapsvr::wire::WriteString(b,"");WritePOD<std::uint8_t>(b,0);WritePOD<std::uint8_t>(b,0);tmapsvr::wire::WriteString(b,"source comment");
    return b;
}
Bytes CharacterMetadata() {
    Bytes b;WritePOD(b,kChar);WritePOD(b,kKey);WritePOD<std::uint32_t>(b,0);WritePOD<std::uint8_t>(b,3);
    tmapsvr::wire::WriteString(b,"");for(int i=0;i<3;++i)WritePOD<std::uint32_t>(b,0);
    tmapsvr::wire::WriteString(b,"");WritePOD<std::uint8_t>(b,0);WritePOD<std::uint8_t>(b,0);
    WritePOD<std::uint16_t>(b,0);WritePOD<std::uint8_t>(b,0);WritePOD<std::uint16_t>(b,0);WritePOD<std::uint8_t>(b,0);
    WritePOD<std::uint32_t>(b,0);WritePOD<std::uint16_t>(b,0);WritePOD<std::uint32_t>(b,0);WritePOD<std::int32_t>(b,0);
    return b;
}
void ReagentSelection() {
    using namespace tmapsvr;
    CharSnapshot s;auto p=std::make_shared<CharacterPayload>();
    ItemInstance item;item.wItemID=8412;item.bCount=2;item.source=std::make_shared<transfer::Item>();item.durable_hash=std::string(64,'a');
    item.dlID=3;item.bInvenID=255;item.bItemID=0;p->bags.push_back({{255,3,0,0},{item}});
    item.dlID=2;item.bInvenID=4;item.bItemID=8;
    auto first=item;first.dlID=1;first.bItemID=2;
    p->bags.push_back({{4,3,0,0},{item,first}});s.payload=p;
    Check(FindSkillReagent(s,8412)->dlID==1,"reagent selection follows unsigned bag and slot order despite reversed vectors");
    auto after=s;ConsumeReagentProjection(after,first);
    Check(s.payload->bags[1].items[1].bCount==2&&after.payload->bags[1].items[1].bCount==1&&after.payload->bags[0].items[0].bCount==2,
          "reagent plan is isolated from live payload and changes only selected item");
    Check(!FindSkillReagent(s,42),"missing reagent has no fallback item");
    p->bags[1].items[1].bCount=0;
    bool failed=false;try{FindSkillReagent(s,8412);}catch(...){failed=true;}
    Check(failed,"invalid zero stack refuses consumption without skipping to a later stack");
    p->bags.clear();item.bInvenID=254;p->bags.push_back({{254,0,0,0},{item}});
    failed=false;try{FindSkillReagent(s,8412);}catch(...){failed=true;}
    Check(failed,"equipped reagent refuses unsupported equipment mutation");
    p->bags.clear();s.dwCharID=42;item.bInvenID=255;item.bCount=2;
    auto raw=std::make_shared<transfer::Item>();raw->id=item.dlID;raw->count=2;raw->item=8412;raw->storage_id=255;raw->owner_id=42;
    raw->texture=0xfedcba98;item.wCustomTex=0xba98;item.source=raw;
    p->bags.push_back({{255,3,0,0},{item}});
    auto graph=std::make_shared<transfer::State>();graph->quests.push_back({12,12345,1,1,1});p->transfer_state=graph;
    const auto selected=*FindSkillReagent(s,8412);after=s;ConsumeReagentProjection(after,selected);
    SkillCooldownTracker timers;after=transfer::PersistenceSnapshot(after,99,timers,0);
    Check(raw->count==2&&after.payload->bags[0].items[0].source->count==1&&
          after.payload->transfer_state->items[0].count==1&&after.payload->transfer_state->items[0].texture==0xfedcba98&&
          after.payload->transfer_state->quests[0].remaining==12345,
          "graph reagent plan retains raw extensions and unrelated state without mutating its source");
    // Bag 4 has only one arrow; bag 255 has two different arrow templates.
    // Exact source order skips the insufficient bag, without combining bags.
    item.bKind=24;item.bInvenID=4;item.bCount=1;item.dlID=10;item.bItemID=0;
    auto a=item;a.bInvenID=255;a.dlID=11;a.bItemID=2;
    auto b=a;b.dlID=12;b.bItemID=3;b.bCount=4;b.wItemID=11054;
    p->bags={{{255,0,0,0},{b,a}},{{4,0,0,0},{item}}};s.payload=p;
    const auto batch=FindSkillAmmunition(s,24,4);
    Check(batch.size()==2&&batch[0].before.dlID==11&&batch[0].count==1&&batch[1].before.dlID==12&&batch[1].count==3,
          "ammunition uses first sufficient bag and ordered same-kind mixed-template stacks");
    after=s;ConsumeSkillItemProjection(after,batch);
    Check(after.payload->bags[0].items.size()==1&&after.payload->bags[0].items[0].bCount==1&&s.payload->bags[0].items.size()==2,
          "batch projection deletes whole first stack and partially decrements second in isolation");
    Check(FindSkillAmmunition(s,24,6).empty(),"insufficient bags cannot be combined even when their total covers the cast");
    b.bCount=255;a.bCount=1;
    Check(SelectSkillAmmunition({a,b},24,2).empty(),"source BYTE accumulation wraps before checking per-bag sufficiency");
    auto c=b;c.dlID=13;c.bItemID=4;c.bCount=2;
    const auto wrapped=SelectSkillAmmunition({c,b,a},24,2);
    Check(wrapped.size()==2&&wrapped[1].before.dlID==12&&wrapped[1].count==1,
          "source wrapped count may qualify later but consumption still starts at first ordered stack");

}
void TransferReservations() {
    asio::io_context io;tcp::socket socket(io);socket.open(tcp::v4());
    auto session=std::make_shared<tnetlib::AsioSession>(std::move(socket),tnetlib::PeerType::Server);
    tmapsvr::InMemorySessionRegistry registry;tmapsvr::InMemoryCharStateStore state;
    using tmapsvr::SessionPhase;using tmapsvr::MapSessionRole;
    Check(registry.TryBind({41,17,123,1,SessionPhase::Ready},session),"transfer test binds ready primary");
    Check(registry.BeginCheckpoint(session.get())&&!registry.BeginCheckpoint(session.get())&&
          !registry.BeginGameplay(session.get())&&!registry.BeginClose(session.get()),"checkpoint lease excludes newer gameplay and teardown");
    registry.EndOperation(session.get());
    Check(registry.BeginGameplay(session.get()),"in-flight gameplay holds a session operation");
    Check(!registry.BeginCheckpoint(session.get()),"pending gameplay prevents stale periodic checkpoint capture");
    Check(registry.BeginTransfer(session.get(),MapSessionRole::Primary)&&registry.Operations(session.get())==2,
          "transfer phase stops new gameplay while retaining in-flight operation");
    Check(!registry.BeginGameplay(session.get())&&!registry.BeginClose(session.get()),"freeze excludes new gameplay and premature teardown");
    registry.EndOperation(session.get());
    Check(registry.Operations(session.get())==1&&!registry.BeginClose(session.get()),"teardown still waits for ownership transaction publication");
    tmapsvr::CharSnapshot snap;snap.dwCharID=41;snap.dwEXP=10;state.Store(41,snap);
    auto frozen=state.Freeze(41,[](auto& s){s.dwEXP=11;});state.Update(41,[](auto& s){s.dwEXP=99;});
    Check(frozen&&state.Get(41)->dwEXP==11&&!state.Freeze(41,[](auto&){}),"frozen snapshot rejects late AI or combat mutation and duplicate freeze");
    registry.EndOperation(session.get());
    Check(!registry.FinishTransfer(session.get(),MapSessionRole::Replica,0)&&
          registry.FinishTransfer(session.get(),MapSessionRole::Replica,1),"demotion requires a newer authority epoch");
    state.Store(41,*frozen);state.Update(41,[](auto& s){s.dwEXP=12;});
    Check(state.Get(41)->dwEXP==12,"explicit replica publication releases frozen state");
    Check(registry.BeginTransfer(session.get(),MapSessionRole::Replica),"same retained socket can receive a return transfer");
    session->Close();
    Check(!registry.BeginClose(session.get())&&registry.FinishTransfer(session.get(),MapSessionRole::Primary,2),
          "closed socket still receives committed promotion before teardown");
    registry.EndOperation(session.get());auto closed=registry.BeginClose(session.get());
    Check(closed&&closed->role==MapSessionRole::Primary&&closed->authority_epoch==2&&closed->phase==SessionPhase::Loaded,
          "teardown acquires final promoted identity and authority atomically");
    Check(!registry.BeginTransfer(session.get(),MapSessionRole::Primary),"closing session cannot begin a later transfer");
}
void TransferDrain() {
    for(bool disconnect:{true,false}) {
        asio::io_context io;tcp::socket socket(io);socket.open(tcp::v4());
        auto session=std::make_shared<tnetlib::AsioSession>(std::move(socket),tnetlib::PeerType::Server);
        tmapsvr::InMemorySessionRegistry registry;tmapsvr::InMemoryCharStateStore state;
        Players players;World world;tmapsvr::SkillCooldownTracker timers;
        tmapsvr::HandlerContext ctx;ctx.session_reg=&registry;ctx.char_state=&state;
        ctx.player_service=&players;ctx.world_client=&world;ctx.skill_cooldown=&timers;
        registry.TryBind({41,17,123,1,tmapsvr::SessionPhase::Ready},session);
        registry.BeginGameplay(session.get()); // a different recipient's blocked send
        tmapsvr::CharSnapshot snap;snap.dwCharID=41;snap.payload=std::make_shared<tmapsvr::CharacterPayload>();state.Store(41,snap);
        Bytes request;WritePOD<std::uint32_t>(request,41);WritePOD<std::uint32_t>(request,123);
        WritePOD<std::uint8_t>(request,1);WritePOD<std::uint16_t>(request,0);
        WritePOD<float>(request,1);WritePOD<float>(request,0);WritePOD<float>(request,1);
        asio::steady_timer stop(io);stop.expires_after(disconnect?20ms:4000ms);
        stop.async_wait([&](auto ec){if(!ec){if(disconnect)session->Close();else io.stop();}});
        bool finished=false;std::exception_ptr error;
        asio::co_spawn(io,tmapsvr::OnMWReleaseMainReq(request,ctx),[&](std::exception_ptr e){error=e;finished=true;stop.cancel();});
        io.run();if(error)std::rethrow_exception(error);
        Check(finished&&!session->IsOpen(),disconnect?"closed source interrupts transfer gameplay drain":"blocked gameplay cannot indefinitely stall World transfer dispatch");
        Check(registry.Operations(session.get())==1&&state.Get(41)->fPosX==0,
              "aborted drain releases only transfer operation and never captures a partial gameplay state");
        registry.EndOperation(session.get());
    }
}
int main(int argc, char**) {
    if (argc > 1) transport_secret.assign(64, std::byte{0x5a});
    try {
        Vectors(); AuditRedaction(); ReagentSelection(); TransferReservations(); TransferDrain(); asio::io_context io; asio::thread_pool workers(2);
        Validator validator; Players players; World world;
        tmapsvr::InMemorySessionRegistry registry; tmapsvr::InMemoryCharStateStore state;
        tmapsvr::InMemoryChannelPresence presence;
        tmapsvr::SkillCooldownTracker timers;
        tmapsvr::HandlerContext ctx; ctx.validator=&validator; ctx.player_service=&players;
        ctx.world_client=&world; ctx.session_reg=&registry; ctx.char_state=&state;
        ctx.presence=&presence; ctx.skill_cooldown=&timers; ctx.expected_group=3; ctx.db_pool=&workers;
        tmapsvr::MapServerConfig cfg; cfg.port=0; cfg.pre_auth_timeout_seconds=1; cfg.handlers=ctx; cfg.rc4_secret_key=transport_secret;
        tmapsvr::MapServer server(io,cfg);
        asio::co_spawn(io,server.Run(),asio::detached);
        std::exception_ptr error;
        asio::co_spawn(io,[&]() -> asio::awaitable<void> {
            // Each malformed request uses a separate socket and must close.
            for (int scenario=0;scenario<9;++scenario) {
                auto c=Dial(io,server.Port()); auto b=Connect();
                const int before=validator.calls;
                int expected=-1;
                if (scenario==0 || scenario==7) { b[0]=std::byte{0}; expected=4; }
                if (scenario==1) b.back()^=std::byte{1};
                if (scenario==2) b.pop_back();
                if (scenario==3) b.push_back(std::byte{0});
                if (scenario==4) { b=Connect(43); expected=2; }
                if (scenario==5) { world.connected=false; expected=5; }
                if (scenario==8) { world.registered=false; expected=5; }
                if (scenario==6) { validator.info.bChannel=9; expected=1; }
                co_await Send(c,MessageId::CS_CONNECT_REQ,std::move(b));
                if (scenario==7) co_await Send(c,MessageId::CS_CONNECT_REQ,Connect());
                co_await Until([&]{return c->ended;},"rejected admission closes socket");
                if (expected<0) Check(c->packets.empty(),"malformed/checksum request has no ACK");
                else Check(c->packets.size()==1 && c->packets[0].first==static_cast<std::uint16_t>(MessageId::CS_CONNECT_ACK) &&
                    c->packets[0].second==Bytes{std::byte(expected),std::byte{0}},"source CN result and empty server list exact bytes");
                if (scenario<4 || scenario==5 || scenario==7 || scenario==8) Check(validator.calls==before,"invalid framing/version/checksum or unavailable World never touches database");
                Check(registry.Size()==0,"failed admission reserves no character");
                world.connected=true; world.registered=true; validator.info.bChannel=2;
            }
            // A rejected durable claim was never announced to World and must
            // not remove an already-online character on a different Map.
            for (int failure=1; failure<=2; ++failure) {
                validator.claim_failure=failure;world.packets.clear();
                auto rejected=Dial(io,server.Port());
                co_await Send(rejected,MessageId::CS_CONNECT_REQ,Connect());
                co_await Until([&]{return rejected->ended && server.LiveSessions()==0;},"failed durable claim completes teardown");
                Check(rejected->packets.size()==1 && rejected->packets[0].second==Bytes{std::byte(failure==1?2:5),std::byte{0}},"claim rejection or exception preserves exact client error");
                Check(world.packets.empty(),"unannounced claim failure sends neither ADDCHAR nor CLOSECHAR to World");
                Check(registry.Size()==0,"failed durable claim releases only its local reservation");
            }
            validator.claim_failure=0;
            // Pipelining CONNECT twice cannot launch concurrent authentication.
            validator.slow=true; auto pipeline=Dial(io,server.Port()); const int before=validator.calls;
            co_await Send(pipeline,MessageId::CS_CONNECT_REQ,Connect());
            co_await Send(pipeline,MessageId::CS_CONNECT_REQ,Connect());
            co_await Until([&]{return pipeline->ended;},"second CONNECT closes original connection");
            Check(validator.calls==before+1,"per-connection dispatch awaits authentication"); validator.slow=false;
            co_await Until([&]{return server.LiveSessions()==0;},"pipelined connection cleanup completes");

            auto c=Dial(io,server.Port()); world.packets.clear();
            co_await Send(c,MessageId::CS_CONNECT_REQ,Connect());
            co_await Until([&]{return registry.Size()==1;},"valid request reserves character");
            co_await Until([&]{return world.packets.size()==1;},"valid request announces to World");
            Check(world.packets[0].first==static_cast<std::uint16_t>(MessageId::MW_ADDCHAR_ACK) && world.packets[0].second.size()==18,"original MW_ADDCHAR_ACK layout");
            Check(c->packets.empty() && !presence.FindEntry(kChar),"no optimistic ACK or AOI visibility while pending");
            auto duplicate=Dial(io,server.Port());
            co_await Send(duplicate,MessageId::CS_CONNECT_REQ,Connect());
            co_await Until([&]{return duplicate->ended;},"duplicate socket rejected");
            Check(duplicate->packets.size()==1 && duplicate->packets[0].second[0]==std::byte{3},"duplicate uses CN_ALREADYEXIST");
            Check(!c->ended && registry.Size()==1,"duplicate cannot replace or evict reservation");
            Bytes enter; WritePOD<std::uint8_t>(enter,1); WritePOD(enter,kChar); WritePOD(enter,kKey);
            co_await tmapsvr::OnMWEnterSvrReq(enter,ctx);
            Check(players.loads==1 && state.Get(kChar).has_value(),"World enter loads character once through worker");
            co_await tmapsvr::OnMWEnterSvrReq(enter,ctx);
            Check(players.loads==1,"repeated enter does not load or overwrite live state");
            co_await tmapsvr::OnMWConResultReq(Verdict(kKey-1),ctx);
            Check(c->packets.empty(),"stale World verdict cannot admit new session");
            co_await tmapsvr::OnMWConResultReq(Verdict(kKey),ctx);
            co_await Until([&]{return c->Count(MessageId::CS_CONNECT_ACK)==1;},"authoritative World verdict produces one client ACK");
            Check(c->packets[0].second==Bytes{std::byte{0},std::byte{2},std::byte{4},std::byte{7}},"client ACK preserves World server IDs");
            co_await tmapsvr::OnMWConResultReq(Verdict(kKey),ctx);
            co_await Pause(15ms);
            Check(c->Count(MessageId::CS_CONNECT_ACK)==1,"duplicate World verdict produces no second ACK");
            Bytes close; WritePOD(close,kChar); WritePOD(close,kKey-1);
            co_await tmapsvr::OnMWCloseCharReq(close,ctx);
            Check(!c->ended && registry.Find(kChar),"stale World close cannot affect new session");
            co_await Send(c,MessageId::CS_CONREADY_REQ,{});
            co_await Until([&]{return c->Count(MessageId::CS_CHARINFO_ACK)==1;},"admitted CONREADY dispatches character info");
            Check(presence.FindEntry(kChar).has_value(),"presence published after admission and CONREADY");
            presence.UpdatePosition(kChar,2010,{3700,87,600});
            players.hold_save=true;
            server.CloseSessions();
            co_await Until([&]{return players.save_started.load();},"disconnect starts awaited save");
            Check(server.LiveSessions()==1 && registry.Find(kChar) && state.Get(kChar),"active count, reservation and snapshot retained during save");
            Check(!presence.FindEntry(kChar),"disconnect is invisible to AOI while save waits");
            auto during_save=Dial(io,server.Port());
            co_await Send(during_save,MessageId::CS_CONNECT_REQ,Connect());
            co_await Until([&]{return during_save->ended;},"reconnect cannot overtake outstanding save");
            Check(during_save->packets[0].second[0]==std::byte{3},"saving reservation rejects duplicate");
            Bytes during_teardown;WritePOD(during_teardown,kChar);WritePOD(during_teardown,kKey);
            WritePOD<std::uint8_t>(during_teardown,1);WritePOD<std::uint8_t>(during_teardown,1);
            co_await tmapsvr::DispatchWorld(static_cast<std::uint16_t>(MessageId::MW_DELCHAR_REQ),during_teardown,ctx);
            players.hold_save=false;
            co_await Until([&]{return server.LiveSessions()==0;},"save completes before active count reaches zero");
            Check(players.saves==1 && players.saved.fPosX==3700 && players.saved.fPosZ==600,"latest live position saved once");
            Check(registry.Size()==0 && !state.Get(kChar),"successful save releases state and reservation");
            Check(std::none_of(world.packets.begin(),world.packets.end(),[](const auto& p){return p.first==static_cast<std::uint16_t>(MessageId::MW_CLOSECHAR_ACK);}),"retirement during delayed final save suppresses close-all echo");
            // A pending handshake is covered by the admission deadline too.
            auto stalled=Dial(io,server.Port());
            co_await Send(stalled,MessageId::CS_CONNECT_REQ,Connect());
            co_await Until([&]{return stalled->ended;},"World-stalled admission closes at deadline",1800ms);
            Check(stalled->packets.empty() && players.saves==1,"timeout never claims success or saves unentered character");
            // Gameplay before CONNECT is rejected by the real dispatch path.
            auto unauth=Dial(io,server.Port()); co_await Send(unauth,MessageId::CS_MOVE_REQ,{});
            co_await Until([&]{return unauth->ended;},"unauthenticated gameplay is closed");
            // World verdict arriving before a complete load cannot claim success.
            auto early=Dial(io,server.Port()); co_await Send(early,MessageId::CS_CONNECT_REQ,Connect());
            co_await Until([&]{return registry.Size()==1;},"early-verdict test reserves character");
            co_await tmapsvr::OnMWConResultReq(Verdict(kKey),ctx);
            co_await Until([&]{return early->ended;},"success verdict before load is rejected");
            Check(early->packets.size()==1 && early->packets[0].second[0]==std::byte{5},"incomplete load returns CN_INTERNAL");
            co_await Until([&]{return server.LiveSessions()==0;},"all connections drained");
            // World retirement is terminal for the exact generation; malformed
            // or stale packets never affect it, and no close-all echo is sent.
            for (int ready=0; ready<2; ++ready) for (int command=0; command<3; ++command) {
                world.packets.clear(); const int saves_before=players.saves;
                auto retired=Dial(io,server.Port());
                co_await Send(retired,MessageId::CS_CONNECT_REQ,Connect());
                co_await Until([&]{return world.packets.size()==1;},"retirement fixture announced");
                if (ready) {
                    co_await tmapsvr::OnMWEnterSvrReq(enter,ctx);
                    co_await tmapsvr::OnMWConResultReq(Verdict(kKey),ctx);
                    co_await Send(retired,MessageId::CS_CONREADY_REQ,{});
                    co_await Until([&]{return retired->Count(MessageId::CS_CHARINFO_ACK)==1;},"retirement fixture ready");
                }
                const auto opcode=command==0?MessageId::MW_INVALIDCHAR_REQ:command==1?MessageId::MW_DELCHAR_REQ:MessageId::MW_CLOSECHAR_REQ;
                Bytes request;WritePOD(request,kChar);WritePOD(request,kKey);
                if(command<2)WritePOD<std::uint8_t>(request,0);
                if(command==1)WritePOD<std::uint8_t>(request,0);
                auto bad=request;bad[4]^=std::byte{1};
                co_await tmapsvr::DispatchWorld(static_cast<std::uint16_t>(opcode),bad,ctx);
                bad=request;bad.pop_back();
                co_await tmapsvr::DispatchWorld(static_cast<std::uint16_t>(opcode),bad,ctx);
                bad=request;bad.push_back(std::byte{0});
                co_await tmapsvr::DispatchWorld(static_cast<std::uint16_t>(opcode),bad,ctx);
                if(command<2){bad=request;bad[8]=std::byte{2};co_await tmapsvr::DispatchWorld(static_cast<std::uint16_t>(opcode),bad,ctx);}
                Check(registry.Find(kChar) && !retired->ended,"wrong-key/truncated/trailing/non-boolean retirement leaves current session alive");
                const auto sent_before=retired->packets.size();
                co_await tmapsvr::DispatchWorld(static_cast<std::uint16_t>(opcode),request,ctx);
                co_await Until([&]{return retired->ended && server.LiveSessions()==0;},"World retirement drains local generation");
                if(command==1)Check(retired->packets.size()==sent_before,"DELCHAR closes without invented client ACK");
                else Check(retired->packets.size()==sent_before+1 && retired->packets.back().second.empty() &&
                    retired->packets.back().first==static_cast<std::uint16_t>(command==0?MessageId::CS_INVALIDCHAR_ACK:MessageId::CS_SHUTDOWN_ACK),"exact empty terminal client opcode");
                Check(std::none_of(world.packets.begin(),world.packets.end(),[](const auto& p){return p.first==static_cast<std::uint16_t>(MessageId::MW_CLOSECHAR_ACK);}),"World-initiated retirement never echoes close-all");
                Check(players.saves==saves_before+ready && registry.Size()==0 && !state.Get(kChar),"ready retirement saves once; pending retirement never saves");
            }
            world.packets.clear();
            auto rejected_by_world=Dial(io,server.Port());
            co_await Send(rejected_by_world,MessageId::CS_CONNECT_REQ,Connect());
            co_await Until([&]{return world.packets.size()==1;},"World rejection fixture announced");
            co_await tmapsvr::OnMWConResultReq(Verdict(kKey,2),ctx);
            co_await Until([&]{return rejected_by_world->ended && server.LiveSessions()==0;},"World negative connect verdict retires local claim");
            Check(world.packets.size()==1,"World rejection does not echo CLOSECHAR");
            validator.info.role=tmapsvr::MapSessionRole::Replica;
            for(int variant=0;variant<4;++variant) {
                world.packets.clear();const int saves=players.saves,loads=players.loads,releases=validator.replica_releases;
                auto replica=Dial(io,server.Port());co_await Send(replica,MessageId::CS_CONNECT_REQ,Connect());
                co_await Until([&]{return world.packets.size()==1;},"replica claim announces its exact World identity");
                co_await tmapsvr::OnMWEnterCharReq(ReplicaComposite(kKey-1),ctx);
                Check(!state.Get(kChar)&&!replica->ended,"wrong-key replica composite cannot hydrate current connection");
                auto body=ReplicaComposite();
                if(variant==1)body.pop_back();
                if(variant==2)body.push_back(std::byte{0});
                if(variant==3) { // valid syntax, unauthorized source location
                    const float wrong=4079;std::memcpy(body.data()+22,&wrong,sizeof(wrong));
                }
                co_await tmapsvr::OnMWEnterCharReq(std::move(body),ctx);
                if(variant==0) {
                    auto snap=state.Get(kChar);
                    Check(snap&&!snap->payload&&snap->szNAME=="Replica"&&snap->cluster.comment=="source comment"&&snap->cluster.aid_country==3,"replica stores exact source summary without fabricating a primary graph");
                    Check(world.packets.size()==2&&world.packets.back().first==static_cast<std::uint16_t>(MessageId::MW_ENTERCHAR_ACK),"replica sends original ENTERCHAR confirmation");
                    co_await Send(replica,MessageId::CS_CONREADY_REQ,{});
                    co_await Until([&]{return presence.FindEntry(kChar).has_value();},"replica becomes ready without its own CONRESULT");
                    Check(registry.ReadySessions().empty()&&validator.replica_readies==1,"replica is excluded from primary checkpoint sweep");
                    Check(replica->packets.empty(),"replica emits neither CONNECT nor CHARINFO success");
                    Bytes del;WritePOD(del,kChar);WritePOD(del,kKey);WritePOD<std::uint8_t>(del,1);WritePOD<std::uint8_t>(del,0);
                    co_await tmapsvr::DispatchWorld(static_cast<std::uint16_t>(MessageId::MW_DELCHAR_REQ),del,ctx);
                }
                co_await Until([&]{return replica->ended&&server.LiveSessions()==0;},"replica lifecycle drains its local connection");
                Check(players.saves==saves&&players.loads==loads&&validator.replica_releases==releases+1,"replica releases its own claim without loading or saving primary data");
                Check(registry.Size()==0&&!state.Get(kChar),"replica cleanup retains no local graph or reservation");
            }
            validator.info.role=tmapsvr::MapSessionRole::Primary;
            world.packets.clear();players.native_payload=true;players.admission_timers=true;
            auto native=Dial(io,server.Port());co_await Send(native,MessageId::CS_CONNECT_REQ,Connect());
            co_await Until([&]{return world.packets.size()==1;},"native primary order fixture announced");
            co_await tmapsvr::OnMWEnterSvrReq(enter,ctx);
            const auto loaded=state.Get(kChar);
            // Inject an earlier local restore origin instead of sleeping three
            // seconds. The immutable payload still holds its database values.
            timers.Restore(kChar,loaded->payload->skills,tmapsvr::SkillClockMs()-3000);
            co_await tmapsvr::DispatchWorld(static_cast<std::uint16_t>(MessageId::MW_CHARINFO_REQ),CharacterMetadata(),ctx);
            co_await Until([&]{return native->Count(MessageId::CS_CHARINFO_ACK)==1;},"native CHARINFO arrives before World confirms CONNECT");
            const auto wire_skills=AdmissionSkills(*native);
            Check(wire_skills.size()==3&&wire_skills[0].wSkillID==7&&wire_skills[0].bLevel==3&&
                  wire_skills[0].dwRemainTick>295000&&wire_skills[0].dwRemainTick<=297000&&wire_skills[1].dwRemainTick==0,
                  "CHARINFO samples current duration and expiration after World admission delay");
            Check(native->packets.size()==2&&native->packets.front().first==static_cast<std::uint16_t>(MessageId::CS_CHGCHANNEL_ACK),"source channel then character hydration order is exact");
            co_await tmapsvr::DispatchWorld(static_cast<std::uint16_t>(MessageId::MW_CHARINFO_REQ),CharacterMetadata(),ctx);
            co_await tmapsvr::OnMWConResultReq(Verdict(kKey),ctx);
            co_await Until([&]{return native->Count(MessageId::CS_CONNECT_ACK)==1;},"CONNECT follows complete client character hydration");
            Check(native->packets.size()==3&&native->Count(MessageId::CS_CHARINFO_ACK)==1,"duplicate World metadata cannot reset the client");
            co_await Pause(30ms);
            co_await Send(native,MessageId::CS_CONREADY_REQ,{});
            co_await Until([&]{return presence.FindEntry(kChar).has_value();},"native CONREADY completes after prior CHARINFO");
            const auto ready_skills=validator.ready_snapshot.payload->skills;
            Check(ready_skills[0].dwRemainTick>295000&&ready_skills[0].dwRemainTick+20<wire_skills[0].dwRemainTick&&ready_skills[1].dwRemainTick==0,
                  "initial checkpoint independently samples live timers after client admission delay");
            Check(loaded->payload->skills[0].dwRemainTick==300000&&state.Get(kChar)->payload->skills[0].dwRemainTick==300000&&
                  timers.RemainMs(kChar,7,tmapsvr::SkillClockMs())<=ready_skills[0].dwRemainTick,
                  "wire and ready sampling neither mutate loaded payload nor restart live timers");
            const auto ready_count=validator.primary_readies.load();
            co_await Send(native,MessageId::CS_CONREADY_REQ,{});
            co_await Pause(10ms);
            Check(native->Count(MessageId::CS_CHARINFO_ACK)==1&&validator.primary_readies==ready_count,"duplicate CONREADY never repeats CHARINFO or initial checkpoint");
            timers.Forget(kChar);players.admission_timers=false;
            // Native skill authority, learned rank and atomic resource gates.
            auto skill_request=[&](std::uint16_t skill,std::uint32_t caster=kChar) {
                Bytes b;WritePOD(b,caster);WritePOD<std::uint8_t>(b,1);WritePOD<std::uint8_t>(b,2);
                WritePOD<std::uint16_t>(b,2010);WritePOD(b,skill);WritePOD<std::uint8_t>(b,0);
                WritePOD<std::uint32_t>(b,0);WritePOD<std::uint32_t>(b,0);
                WritePOD<float>(b,0);WritePOD<float>(b,0);WritePOD<float>(b,0);WritePOD<std::uint8_t>(b,0);return b;
            };
            auto verdict=[&](std::uint8_t expected) {
                for(auto it=native->packets.rbegin();it!=native->packets.rend();++it)
                    if(it->first==static_cast<std::uint16_t>(MessageId::CS_SKILLUSE_ACK))return it->second[0]==std::byte(expected);
                return false;
            };
            const auto baseline=state.Get(kChar);
            co_await Send(native,MessageId::CS_SKILLUSE_REQ,skill_request(999));
            co_await Until([&]{return native->Count(MessageId::CS_SKILLUSE_ACK)==1;},"unlearned native cast receives verdict");
            Check(verdict(tmapsvr::SKILL_NOTFOUND)&&state.Get(kChar)->dwMP==baseline->dwMP&&timers.Snapshot(kChar,tmapsvr::SkillClockMs()).empty(),
                  "unlearned cast returns original NOTFOUND without consuming MP or timer");
            // Ignored malformed/spoofed requests precede an unknown-skill reply
            // as a receive barrier; none may cause an extra cast response.
            auto truncated=skill_request(7);truncated.back()=std::byte{1};
            co_await Send(native,MessageId::CS_SKILLUSE_REQ,truncated);
            auto trailing=skill_request(7);trailing.push_back(std::byte{0});
            co_await Send(native,MessageId::CS_SKILLUSE_REQ,trailing);
            co_await Send(native,MessageId::CS_SKILLUSE_REQ,skill_request(7,kChar+1));
            co_await Send(native,MessageId::CS_SKILLUSE_REQ,skill_request(999));
            co_await Until([&]{return native->Count(MessageId::CS_SKILLUSE_ACK)>=2;},"malformed cast barrier answered");
            Check(native->Count(MessageId::CS_SKILLUSE_ACK)==2&&state.Get(kChar)->dwMP==baseline->dwMP,
                  "truncated targets trailing bytes and foreign caster cannot execute a native cast");
            co_await Send(native,MessageId::CS_SKILLUSE_REQ,skill_request(7));
            co_await Until([&]{return native->Count(MessageId::CS_HPMP_ACK)==1;},"native charged cast returns skill and bars");
            const auto charged=state.Get(kChar);
            Check(verdict(tmapsvr::SKILL_SUCCESS)&&charged->dwMP==83&&charged->dwHP==153,
                  "native rank3 MP cost and percentage HP cost are charged exactly once");
            for(const auto& packet:native->packets)if(packet.first==static_cast<std::uint16_t>(MessageId::CS_SKILLUSE_ACK)&&packet.second[0]==std::byte{0})
                Check(packet.second[19]==std::byte{3},"native success ACK carries actual learned rank");
            Check(timers.RemainMs(kChar,7,tmapsvr::SkillClockMs())>47000&&timers.RemainMs(kChar,7,tmapsvr::SkillClockMs())<=48800,
                  "native use arms rank and attack-speed scaled cooldown without optional chart");
            Check(timers.RemainMs(kChar,8,tmapsvr::SkillClockMs())>3000&&timers.RemainMs(kChar,8,tmapsvr::SkillClockMs())<=4000&&timers.RemainMs(kChar,9,tmapsvr::SkillClockMs())==0,
                  "native use arms only learned peers of the same kind without scaling kind delay");
            state.Update(kChar,[](auto& v){v.dwMP=79;});
            co_await Send(native,MessageId::CS_SKILLUSE_REQ,skill_request(7));
            co_await Until([&]{return native->Count(MessageId::CS_SKILLUSE_ACK)==4;},"MP rejection answered");
            Check(verdict(tmapsvr::SKILL_NEEDMP)&&state.Get(kChar)->dwMP==79&&state.Get(kChar)->dwHP==153,
                  "insufficient MP precedes cooldown and cannot charge either resource");
            state.Update(kChar,[](auto& v){v.dwMP=100;v.dwHP=16;});
            co_await Send(native,MessageId::CS_SKILLUSE_REQ,skill_request(7));
            co_await Until([&]{return native->Count(MessageId::CS_SKILLUSE_ACK)==5;},"HP rejection answered");
            Check(verdict(tmapsvr::SKILL_NEEDHP)&&state.Get(kChar)->dwMP==100&&state.Get(kChar)->dwHP==16,
                  "equal HP and cost cannot kill caster or consume MP");
            state.Update(kChar,[](auto& v){v.dwHP=169;});
            co_await Send(native,MessageId::CS_SKILLUSE_REQ,skill_request(7));
            co_await Until([&]{return native->Count(MessageId::CS_SKILLUSE_ACK)==6;},"cooldown rejection answered");
            Check(verdict(tmapsvr::SKILL_SPEEDYUSE)&&state.Get(kChar)->dwMP==100&&native->Count(MessageId::CS_HPMP_ACK)==1,
                  "cooldown rejection does not deduct resources or emit changed bars");
            Check(timers.RemainMs(kChar,7,tmapsvr::SkillClockMs())<=48800&&timers.RemainMs(kChar,7,tmapsvr::SkillClockMs())>47000,
                  "resource rejections preserve existing live cooldown");
            // Ordinary source gate ordering: region, MP, HP, previous active
            // effect, reuse, then item eligibility. The item rejection follows
            // SkillUse and therefore retains own AND shared-kind timers.
            std::size_t ordinary_replies=native->Count(MessageId::CS_SKILLUSE_ACK);
            auto ordinary_verdict=[&](std::uint16_t skill,std::uint8_t code)->asio::awaitable<void> {
                co_await Send(native,MessageId::CS_SKILLUSE_REQ,skill_request(skill));++ordinary_replies;
                co_await Until([&]{return native->Count(MessageId::CS_SKILLUSE_ACK)==ordinary_replies;},"ordinary prerequisite verdict received");
                Check(verdict(code),"ordinary cast follows original gate precedence");
            };
            state.Update(kChar,[](auto& v){
                v.dwMP=0;v.dwHP=0;auto p=std::make_shared<tmapsvr::CharacterPayload>(*v.payload);
                auto& t=p->skill_templates[0];t.wMapID=550;t.wPrevActiveID=8;t.items=tmapsvr::SkillItemGate::Unsuitable;v.payload=p;
            });
            co_await ordinary_verdict(7,tmapsvr::SKILL_WRONGREGION);
            state.Update(kChar,[](auto& v){auto p=std::make_shared<tmapsvr::CharacterPayload>(*v.payload);p->skill_templates[0].wMapID=2010;v.payload=p;});
            co_await ordinary_verdict(7,tmapsvr::SKILL_NEEDMP);
            state.Update(kChar,[](auto& v){v.dwMP=100;v.dwHP=16;});
            co_await ordinary_verdict(7,tmapsvr::SKILL_NEEDHP);
            state.Update(kChar,[](auto& v){v.dwHP=169;});
            co_await ordinary_verdict(7,tmapsvr::SKILL_NEEDPREVACT);
            Check(timers.RemainMs(kChar,7,tmapsvr::SkillClockMs())<=48800,"prerequisite rejection does not extend imported or live cooldown");
            state.Update(kChar,[](auto& v){auto p=std::make_shared<tmapsvr::CharacterPayload>(*v.payload);p->skill_templates[0].wPrevActiveID=0;v.payload=p;});
            co_await ordinary_verdict(7,tmapsvr::SKILL_SPEEDYUSE);
            timers.Forget(kChar);
            co_await ordinary_verdict(7,tmapsvr::SKILL_UNSUITWEAPON);
            Check(timers.RemainMs(kChar,7,tmapsvr::SkillClockMs())>47000&&timers.RemainMs(kChar,7,tmapsvr::SkillClockMs())<=48800&&
                  timers.RemainMs(kChar,8,tmapsvr::SkillClockMs())>3000&&timers.RemainMs(kChar,8,tmapsvr::SkillClockMs())<=4000&&
                  timers.RemainMs(kChar,9,tmapsvr::SkillClockMs())==0,
                  "ordinary unsuitable weapon retains source own and same-kind cooldowns");
            co_await ordinary_verdict(7,tmapsvr::SKILL_SPEEDYUSE);
            Check(state.Get(kChar)->dwMP==100&&state.Get(kChar)->dwHP==169&&native->Count(MessageId::CS_HPMP_ACK)==1,
                  "region prerequisite and weapon rejections never deduct resources or send bars");
            state.Update(kChar,[](auto& v){
                auto p=std::make_shared<tmapsvr::CharacterPayload>(*v.payload);
                auto& t=p->skill_templates[2];t.wMapID=2010;t.wTargetActiveID=8;t.items=tmapsvr::SkillItemGate::Allowed;
                // A source map restriction is specific to ordinary use.
                p->skill_templates[0].wMapID=550;p->skill_templates[0].items=tmapsvr::SkillItemGate::Allowed;v.payload=p;
            });
            co_await ordinary_verdict(9,tmapsvr::SKILL_SUCCESS);
            Check(state.Get(kChar)->dwMP==100&&state.Get(kChar)->dwHP==169,
                  "ordinary use accepts matching source map and ignores loop-only target prerequisite");
            // The loop packet omits action/animation fields, has its own ACK,
            // checks cooldown before costs, and never rearms same-kind peers.
            auto loop_request=[&](std::uint16_t skill=7,std::uint32_t caster=kChar) {
                Bytes b;WritePOD(b,caster);WritePOD<std::uint8_t>(b,1);WritePOD<std::uint8_t>(b,2);
                WritePOD<std::uint16_t>(b,2010);WritePOD(b,skill);
                WritePOD<float>(b,1.25f);WritePOD<float>(b,0);WritePOD<float>(b,-2.5f);WritePOD<std::uint8_t>(b,0);return b;
            };
            std::size_t loop_replies=0;
            auto loop_verdict=[&](Bytes body,std::uint8_t code)->asio::awaitable<void> {
                co_await Send(native,MessageId::CS_LOOPSKILL_REQ,std::move(body));++loop_replies;
                co_await Until([&]{return native->Count(MessageId::CS_LOOPSKILL_ACK)==loop_replies;},"native loop verdict received");
                auto it=std::find_if(native->packets.rbegin(),native->packets.rend(),[](const auto& p){return p.first==static_cast<std::uint16_t>(MessageId::CS_LOOPSKILL_ACK);});
                Check(it->second.size()==45&&it->second[0]==std::byte(code),"loop result has original opcode and 45-byte layout");
            };
            co_await loop_verdict(loop_request(999),tmapsvr::SKILL_NOTFOUND);
            state.Update(kChar,[](auto& v){v.dwMP=0;v.dwHP=0;});
            co_await loop_verdict(loop_request(),tmapsvr::SKILL_SPEEDYUSE);
            timers.Forget(kChar);
            co_await loop_verdict(loop_request(),tmapsvr::SKILL_NEEDMP);
            state.Update(kChar,[](auto& v){v.dwMP=100;v.dwHP=15;});
            co_await loop_verdict(loop_request(),tmapsvr::SKILL_NEEDHP);
            state.Update(kChar,[](auto& v){v.dwHP=16;auto p=std::make_shared<tmapsvr::CharacterPayload>(*v.payload);p->skill_templates[0].wTargetActiveID=8;v.payload=p;});
            co_await loop_verdict(loop_request(),tmapsvr::SKILL_NEEDPREVACT);
            state.Update(kChar,[](auto& v){auto p=std::make_shared<tmapsvr::CharacterPayload>(*v.payload);p->skill_templates[0].wTargetActiveID=0;p->skill_templates[0].items=tmapsvr::SkillItemGate::Unsuitable;v.payload=p;});
            co_await loop_verdict(loop_request(),tmapsvr::SKILL_UNSUITWEAPON);
            Check(state.Get(kChar)->dwHP==16&&state.Get(kChar)->dwMP==100&&timers.Snapshot(kChar,tmapsvr::SkillClockMs()).empty(),"loop rejections never deduct or arm timers; learned prerequisite is not an active effect");
            state.Update(kChar,[](auto& v){auto p=std::make_shared<tmapsvr::CharacterPayload>(*v.payload);p->skill_templates[0].items=tmapsvr::SkillItemGate::Allowed;v.payload=p;});
            auto bad_loop=loop_request();bad_loop.back()=std::byte{1};
            co_await Send(native,MessageId::CS_LOOPSKILL_REQ,bad_loop);
            bad_loop=loop_request();bad_loop.push_back(std::byte{0});
            co_await Send(native,MessageId::CS_LOOPSKILL_REQ,bad_loop);
            co_await Send(native,MessageId::CS_LOOPSKILL_REQ,loop_request(7,kChar+1));
            co_await loop_verdict(loop_request(),tmapsvr::SKILL_SUCCESS);
            co_await Until([&]{return native->Count(MessageId::CS_HPMP_ACK)==2;},"loop resource bars delivered");
            const auto remaining=timers.RemainMs(kChar,7,tmapsvr::SkillClockMs());
            Check(remaining>1500&&remaining<=2000&&timers.RemainMs(kChar,8,tmapsvr::SkillClockMs())==0,
                  "loop uses (2000+500)*80/100 without rank increment or shared-kind extension");
            Check(state.Get(kChar)->dwMP==20&&state.Get(kChar)->dwHP==0,"loop source HP equality is accepted and exact learned-rank costs deducted once");
            co_await loop_verdict(loop_request(),tmapsvr::SKILL_SPEEDYUSE);
            Check(state.Get(kChar)->dwMP==20&&native->Count(MessageId::CS_HPMP_ACK)==2,"loop repeat rejects before resource checks without another charge");
            timers.Forget(kChar);
            state.Update(kChar,[](auto& v){v.dwMP=100;v.dwHP=169;});
            state.Update(kChar,[](auto& v){auto p=std::make_shared<tmapsvr::CharacterPayload>(*v.payload);p->skill_templates[0].wMapID=0xffff;p->skill_attack_timing.reset();v.payload=p;});
            co_await Send(native,MessageId::CS_SKILLUSE_REQ,skill_request(7));
            co_await Until([&]{return native->ended&&server.LiveSessions()==0;},"unsupported native timing closes and durably drains");
            Check(players.saved.dwMP==100&&players.saved.dwHP==169,"unsupported native timing never guesses or charges a cast");
            for(int unsupported=0;unsupported<4;++unsupported) {
                world.packets.clear();const auto saves=players.saves.load();
                auto guarded=Dial(io,server.Port());co_await Send(guarded,MessageId::CS_CONNECT_REQ,Connect());
                co_await Until([&]{return world.packets.size()==1;},"unsupported loop fixture announced");
                co_await tmapsvr::OnMWEnterSvrReq(enter,ctx);
                co_await tmapsvr::DispatchWorld(static_cast<std::uint16_t>(MessageId::MW_CHARINFO_REQ),CharacterMetadata(),ctx);
                co_await tmapsvr::OnMWConResultReq(Verdict(kKey),ctx);
                co_await Send(guarded,MessageId::CS_CONREADY_REQ,{});
                co_await Until([&]{return presence.FindEntry(kChar).has_value();},"unsupported loop fixture ready");
                state.Update(kChar,[&](auto& v){
                    auto p=std::make_shared<tmapsvr::CharacterPayload>(*v.payload);
                    if(unsupported%2==0)p->skill_templates[0].items=tmapsvr::SkillItemGate::Unsupported;
                    else {
                        auto& t=p->skill_templates[0];t.bSpeedApply=0;
                        (unsupported<2?t.wTargetActiveID:t.wPrevActiveID)=8;
                        auto graph=std::make_shared<tmapsvr::transfer::State>();graph->buffs.push_back({});graph->buffs[0].skill=8;
                        p->transfer_state=graph;
                    }
                    v.payload=p;
                });
                co_await Send(guarded,unsupported<2?MessageId::CS_LOOPSKILL_REQ:MessageId::CS_SKILLUSE_REQ,unsupported<2?loop_request():skill_request(7));
                co_await Until([&]{return guarded->ended&&server.LiveSessions()==0;},"unsupported consumable or active-effect cast closes and drains");
                Check(players.saves==saves+1&&players.saved.dwHP==169&&players.saved.dwMP==163&&
                      guarded->Count(MessageId::CS_SKILLUSE_ACK)==0&&
                      guarded->Count(MessageId::CS_LOOPSKILL_ACK)==0&&guarded->Count(MessageId::CS_HPMP_ACK)==0,
                      "unsupported cast never acknowledges success or consumes resources");
                Check(std::all_of(players.saved.payload->skills.begin(),players.saved.payload->skills.end(),[](const auto& skill){return skill.dwRemainTick==0;}),
                      "unsupported cast never arms timers before closing");
            }
            players.reagent_fixture=true;
            for(int mode=0;mode<6;++mode) {
                world.packets.clear();const auto saves=players.saves.load(),consumes=players.consumptions.load();
                const auto failures=server.FailedSaves();
                auto item_client=Dial(io,server.Port());co_await Send(item_client,MessageId::CS_CONNECT_REQ,Connect());
                co_await Until([&]{return world.packets.size()==1;},"reagent fixture announced");
                co_await tmapsvr::OnMWEnterSvrReq(enter,ctx);
                co_await tmapsvr::DispatchWorld(static_cast<std::uint16_t>(MessageId::MW_CHARINFO_REQ),CharacterMetadata(),ctx);
                co_await tmapsvr::OnMWConResultReq(Verdict(kKey),ctx);
                co_await Send(item_client,MessageId::CS_CONREADY_REQ,{});
                co_await Until([&]{return presence.FindEntry(kChar).has_value();},"reagent fixture ready");
                if(mode>=3)state.Update(kChar,[](auto& v){
                    auto p=std::make_shared<tmapsvr::CharacterPayload>(*v.payload);
                    auto& t=p->skill_templates[0];t.items=tmapsvr::SkillItemGate::Ammunition;t.wUseItem=0;t.bAmmoKind=24;
                    p->bags[0].items[0].bKind=24;v.payload=p;
                });
                const auto target_request=[&](Bytes body) {
                    const int hits=mode==4?0:mode==5?2:mode>=3?1:0;
                    body.back()=std::byte(hits);
                    for(int i=0;i<hits;++i){WritePOD(body,std::uint32_t(1234+i));WritePOD(body,std::uint8_t(2));WritePOD(body,std::uint8_t(1));}
                    return body;
                };
                if(mode==4) {
                    const auto body=target_request(mode==4?skill_request(7):loop_request());
                    co_await Send(item_client,mode==4?MessageId::CS_SKILLUSE_REQ:MessageId::CS_LOOPSKILL_REQ,body);
                    co_await Until([&]{return item_client->ended&&server.LiveSessions()==0;},"unsupported ammunition hit count closes before charging");
                    Check(players.consumptions==consumes&&players.saved.dwMP==163&&players.saved.payload->bags[0].items[0].bCount==2&&
                          item_client->Count(MessageId::CS_UPDATEITEM_ACK)==0&&item_client->Count(MessageId::CS_SKILLUSE_ACK)==0&&item_client->Count(MessageId::CS_LOOPSKILL_ACK)==0,
                          "zero ammunition hits never consume inventory or acknowledge success");
                    Check(std::all_of(players.saved.payload->skills.begin(),players.saved.payload->skills.end(),[](const auto& skill){return skill.dwRemainTick==0;}),
                          "unsupported ammunition hit counts preserve all cooldowns");
                    continue;
                }
                if(mode==5) {
                    // Three stacks in reverse DTO order; two are consumed by
                    // one ordinary two-target cast; the remainder cannot fund a loop.
                    state.Update(kChar,[](auto& v){
                        auto p=std::make_shared<tmapsvr::CharacterPayload>(*v.payload);
                        auto item=p->bags[0].items[0];item.bCount=1;
                        p->bags[0].items.clear();
                        for(int slot:{6,5,4}){item.bItemID=slot;item.dlID=slot;p->bags[0].items.push_back(item);}
                        v.payload=p;
                    });
                    co_await Send(item_client,MessageId::CS_SKILLUSE_REQ,target_request(skill_request(7)));
                    co_await Until([&]{return item_client->Count(MessageId::CS_HPMP_ACK)==1;},"multi-stack cast committed");
                    const auto n=item_client->packets.size();
                    Check(item_client->packets[n-5]==std::pair{static_cast<std::uint16_t>(MessageId::CS_DELITEM_ACK),Bytes{std::byte{255},std::byte{4}}}&&
                          item_client->packets[n-4]==std::pair{static_cast<std::uint16_t>(MessageId::CS_DELITEM_ACK),Bytes{std::byte{255},std::byte{5}}}&&
                          item_client->packets[n-3].first==static_cast<std::uint16_t>(MessageId::CS_MOVEITEM_ACK)&&
                          item_client->packets[n-2].second.size()==72&&players.consumptions==consumes+1,
                          "two stack deletions precede one MOVEITEM and original two-target success");
                    timers.Forget(kChar);
                    co_await Send(item_client,MessageId::CS_LOOPSKILL_REQ,target_request(loop_request()));
                    co_await Until([&]{return item_client->Count(MessageId::CS_LOOPSKILL_ACK)==1;},"insufficient multi-hit loop rejected");
                    Check(players.consumptions==consumes+1&&state.Get(kChar)->payload->bags[0].items.size()==1&&
                          timers.Snapshot(kChar,tmapsvr::SkillClockMs()).empty(),"insufficient loop preserves remaining stack and timers");
                    item_client->wire->Close();
                    co_await Until([&]{return item_client->ended&&server.LiveSessions()==0;},"multi-stack fixture closes");
                    continue;
                }
                players.hold_consumption=true;players.consumption_started=false;players.fail_consumption=mode==2;
                auto server_session=registry.Find(kChar,kKey);
                if(mode==0)Check(registry.BeginCheckpoint(server_session.get()),"fixture holds a periodic checkpoint lease");
                co_await Send(item_client,MessageId::CS_SKILLUSE_REQ,target_request(skill_request(7)));
                if(mode==0) {
                    co_await Pause(20ms);
                    Check(!players.consumption_started&&item_client->Count(MessageId::CS_SKILLUSE_ACK)==0,"cast waits for checkpoint without dropping packet");
                    registry.EndOperation(server_session.get());
                }
                co_await Until([&]{return players.consumption_started.load();},"reagent write runs on worker");
                Check(state.Get(kChar)->dwMP==163&&state.Get(kChar)->payload->bags[0].items[0].bCount==2&&
                      item_client->Count(MessageId::CS_UPDATEITEM_ACK)==0&&!registry.BeginCheckpoint(server_session.get()),
                      "uncommitted reagent plan is invisible and excludes periodic capture");
                if(mode==1)item_client->wire->Close();
                players.hold_consumption=false;
                if(mode==0||mode==3) {
                    co_await Until([&]{return item_client->Count(MessageId::CS_HPMP_ACK)==1;},"committed normal reagent cast publishes inventory and bars");
                    Check(players.consumptions==consumes+1&&state.Get(kChar)->dwMP==83&&state.Get(kChar)->payload->bags[0].items[0].bCount==1,
                          "confirmed transaction publishes exact item and resource decrement once");
                    const auto n=item_client->packets.size();
                    Check(item_client->packets[n-4].first==static_cast<std::uint16_t>(MessageId::CS_UPDATEITEM_ACK)&&
                          item_client->packets[n-4].second[0]==std::byte{255}&&item_client->packets[n-4].second[10]==std::byte{1}&&
                          item_client->packets[n-3].first==static_cast<std::uint16_t>(MessageId::CS_MOVEITEM_ACK)&&
                          item_client->packets[n-2].first==static_cast<std::uint16_t>(MessageId::CS_SKILLUSE_ACK),
                          "private UPDATEITEM and MOVEITEM precede cast and HPMP in source order");
                    co_await Send(item_client,MessageId::CS_SKILLUSE_REQ,target_request(skill_request(7)));
                    co_await Until([&]{return item_client->Count(MessageId::CS_SKILLUSE_ACK)==2;},"reagent cooldown rejection delivered");
                    Check(players.consumptions==consumes+1,"cooldown repeat cannot consume another item");
                    timers.Forget(kChar);
                    co_await Send(item_client,MessageId::CS_LOOPSKILL_REQ,target_request(loop_request()));
                    co_await Until([&]{return item_client->Count(MessageId::CS_HPMP_ACK)==2;},"loop consumes final reagent");
                    Check(players.consumptions==consumes+2&&state.Get(kChar)->payload->bags[0].items.empty()&&state.Get(kChar)->dwMP==3,
                          "last stack element disappears with atomic loop costs");
                    const auto del=std::find_if(item_client->packets.begin(),item_client->packets.end(),[](const auto& p){return p.first==static_cast<std::uint16_t>(MessageId::CS_DELITEM_ACK);});
                    Check(del!=item_client->packets.end()&&del->second==Bytes({std::byte{255},std::byte{4}}),"DELITEM retains original two-byte bag/slot layout");
                    timers.Forget(kChar);state.Update(kChar,[](auto& v){v.dwMP=163;});
                    co_await Send(item_client,MessageId::CS_LOOPSKILL_REQ,target_request(loop_request()));
                    co_await Until([&]{return item_client->Count(MessageId::CS_LOOPSKILL_ACK)==2;},"missing loop reagent rejection delivered");
                    Check(timers.Snapshot(kChar,tmapsvr::SkillClockMs()).empty()&&players.consumptions==consumes+2,"missing loop reagent arms no timers or write");
                    co_await Send(item_client,MessageId::CS_SKILLUSE_REQ,target_request(skill_request(7)));
                    co_await Until([&]{return item_client->Count(MessageId::CS_SKILLUSE_ACK)==3;},"missing ordinary reagent rejection delivered");
                    Check(timers.RemainMs(kChar,7,tmapsvr::SkillClockMs())>47000&&players.consumptions==consumes+2,"missing ordinary reagent retains source timer without item write");
                    item_client->wire->Close();
                }
                co_await Until([&]{return item_client->ended&&server.LiveSessions()==0;},"reagent fixture drains after disconnect or transaction failure");
                if(mode==1)Check(players.saves==saves+1&&players.saved.dwMP==83&&players.saved.payload->bags[0].items[0].bCount==1,
                                "disconnect during commit publishes committed state before final save");
                if(mode==2) {
                    Check(players.saves==saves&&players.consumptions==consumes&&server.FailedSaves()==failures+1&&
                          state.Get(kChar)->persistence_uncertain&&registry.Size()==1&&item_client->Count(MessageId::CS_UPDATEITEM_ACK)==0,
                          "unknown commit outcome retains reservation and refuses stale final save without success ACK");
                    // Explicitly simulate a new test process after recovery; production never clears this reservation automatically.
                    registry.Unbind(kChar);state.Remove(kChar);timers.Forget(kChar);
                }
            }
            players.reagent_fixture=false;players.fail_consumption=false;
            for(int variant=0;variant<4;++variant) {
                world.packets.clear();const auto saves=players.saves.load(),readies=validator.primary_readies.load();
                auto invalid=Dial(io,server.Port());co_await Send(invalid,MessageId::CS_CONNECT_REQ,Connect());
                co_await Until([&]{return world.packets.size()==1;},"invalid admission timer fixture announced");
                co_await tmapsvr::OnMWEnterSvrReq(enter,ctx);
                if(variant>=2) {
                    co_await tmapsvr::DispatchWorld(static_cast<std::uint16_t>(MessageId::MW_CHARINFO_REQ),CharacterMetadata(),ctx);
                    co_await tmapsvr::OnMWConResultReq(Verdict(kKey),ctx);
                }
                auto invalid_ctx=ctx;
                if(variant%2==0)invalid_ctx.skill_cooldown=nullptr;
                else timers.TryUse(kChar,65534,tmapsvr::SkillClockMs(),10000);
                if(variant<2)
                    co_await tmapsvr::DispatchWorld(static_cast<std::uint16_t>(MessageId::MW_CHARINFO_REQ),CharacterMetadata(),invalid_ctx);
                else co_await tmapsvr::OnConReadyReq(registry.Find(kChar,kKey),{},invalid_ctx);
                co_await Until([&]{return invalid->ended&&server.LiveSessions()==0;},"missing tracker or unlearned timer refuses native admission and drains");
                Check(validator.primary_readies==readies&&players.saves==saves&&!state.Get(kChar)&&registry.Size()==0,
                      "invalid admission timers cannot create readiness or save unverified state");
                if(variant<2)Check(invalid->Count(MessageId::CS_CHARINFO_ACK)==0,"invalid native timers never emit stale CHARINFO");
            }
            players.native_payload=false;
            // A failed write keeps dirty state and blocks a new login in this process.
            players.save_started=false; players.fail_save=true;
            auto dirty=Dial(io,server.Port()); co_await Send(dirty,MessageId::CS_CONNECT_REQ,Connect());
            co_await Until([&]{return registry.Size()==1;},"save-failure fixture reserved");
            co_await tmapsvr::OnMWEnterSvrReq(enter,ctx);
            co_await tmapsvr::OnMWConResultReq(Verdict(kKey),ctx);
            co_await Until([&]{return dirty->Count(MessageId::CS_CONNECT_ACK)==1;},"save-failure fixture admitted");
            co_await Send(dirty,MessageId::CS_CONREADY_REQ,{});
            co_await Until([&]{return presence.FindEntry(kChar).has_value();},"save-failure fixture ready");
            server.CloseSessions();
            co_await Until([&]{return server.LiveSessions()==0;},"failed save is observed by teardown");
            Check(registry.Size()==1 && state.Get(kChar).has_value() && server.FailedSaves()==2,"failed save retains dirty snapshot and reservation and reports failure");
            auto retry=Dial(io,server.Port()); co_await Send(retry,MessageId::CS_CONNECT_REQ,Connect());
            co_await Until([&]{return retry->ended;},"dirty reservation rejects reconnect");
            Check(retry->packets.size()==1 && retry->packets[0].second[0]==std::byte{3},"failed save cannot be hidden by stale reload");
            co_await Until([&]{return server.LiveSessions()==0;},"final connection drained");
            server.StopAccepting(); io.stop();
        },[&](std::exception_ptr ep){ if(ep) {error=ep; io.stop();} });
        io.run(); workers.join(); if(error)std::rethrow_exception(error);
        std::printf("%d admission checks passed\n",passed); return 0;
    } catch(const std::exception& e) {std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
}
