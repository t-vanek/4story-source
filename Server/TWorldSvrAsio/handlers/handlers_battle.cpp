#include "handlers.h"
#include "../senders/senders.h"
#include "../services/pvp_aggregate.h"
#include "../wire_codec.h"
#include "fourstory/db/co_offload.h"
#include "MessageId.h"
#include <algorithm>
#include <ctime>
#include <mutex>
#include <stdexcept>
#include <boost/asio/use_awaitable.hpp>

namespace tworldsvr::handlers {
namespace {
void Enqueue(const HandlerContext& ctx,const BattleSignal& signal) {
    if(!ctx.battle_queue||!ctx.battle_queue->try_send(boost::system::error_code{},signal)) {
        if(ctx.io)ctx.io->stop();
        throw std::runtime_error("Source battle batch queue unavailable or full; World stopped without dropping/replaying an event");
    }
}
boost::asio::awaitable<void> Publish(const HandlerContext& ctx,const BattleSignal& signal) {
    if(!ctx.peers)throw std::runtime_error("World battle peer registry unavailable");
    for(auto& peer:ctx.peers->Snapshot()) {
        if(ctx.native_battles&&!ctx.peers->IsCurrentMap(peer))continue;
        switch(signal.type) {
        case battle::kTypeLocal:co_await senders::SendMwLocalEnableReq(peer,signal.status,signal.second,0,0,0);break;
        case battle::kTypeCastle:co_await senders::SendMwCastleEnableReq(peer,signal.status,signal.second);break;
        case battle::kTypeMission:co_await senders::SendMwMissionEnableReq(peer,signal.status,signal.start,signal.second);break;
        case battle::kTypeSkyGarden:co_await senders::SendMwSkyGardenEnableReq(peer,signal.status,signal.second,0,signal.start);break;
        default:break;
        }
    }
    // Source performs this after fanout, including unknown/tournament types.
    // These are real RAM mutations; there is no source DB save in this handler.
    if(signal.type!=battle::kTypeMission&&signal.status==4) {
        const auto observed=ctx.battle_clock?*ctx.battle_clock:static_cast<std::int64_t>(std::time(nullptr));
        if(!ctx.guilds||(signal.type==battle::kTypeCastle&&!ctx.castle_war)||observed<0||observed>4000000000LL)
            throw std::runtime_error("World battle peace dependencies unavailable");
        const auto recent=static_cast<std::uint32_t>(observed/86400);
        ctx.guilds->SetRecentRecordDate(recent);
        auto ids=ctx.guilds->SnapshotIds();std::sort(ids.begin(),ids.end());
        for(auto id:ids)if(auto guild=ctx.guilds->Find(id)) {
            std::lock_guard guard(guild->lock);
            for(auto& member:guild->members)CalcWeekRecord(member,recent);
        }
        if(signal.type==battle::kTypeCastle)ctx.castle_war->Clear();
    }
}
}
boost::asio::awaitable<void> RunBattleTick(const HandlerContext& ctx,std::int64_t observed) {
    if(!ctx.battles||!ctx.native_battles)throw std::runtime_error("Native battle scheduler unavailable");
    const auto clock=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
        [repo=ctx.native_battles,observed]{return repo->Observe(observed);});
    // Tick plans on a copy. A source-invalid vector branch never publishes a
    // partly reset state. RAM state is original process-local state, not a save.
    if(const auto signal=ctx.battles->Tick(clock))Enqueue(ctx,*signal);
}
boost::asio::awaitable<void> RunBattleDelivery(const HandlerContext& ctx) {
    if(!ctx.native_battles||!ctx.battle_queue||!ctx.battle_clock)
        throw std::runtime_error("Native battle batch dependencies unavailable");
    while(true) {
        const auto signal=co_await ctx.battle_queue->async_receive(boost::asio::use_awaitable);
        auto operation=co_await ctx.battle_operations->Acquire();
        const auto observed=*ctx.battle_clock;
        const auto current=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
            [repo=ctx.native_battles,observed]{return repo->Observe(observed);});
        (void)current;
        co_await Publish(ctx,signal);
    }
}
boost::asio::awaitable<void> OnBattleStatusReq(std::shared_ptr<PeerSession> peer,
    std::vector<std::byte> body,const HandlerContext& ctx) {
    const auto authorized=[&]{return ctx.ctrl_svr&&ctx.ctrl_svr->Get()==peer&&peer->Wire()->IsOpen();};
    if(ctx.native_battles&&!authorized()){peer->Wire()->Close();co_return;}
    wire::Reader reader(body.data(),body.size());BattleSignal signal;
    if(!reader.Read(signal.type)||!reader.Read(signal.status)||!reader.Read(signal.start)||
       !reader.Read(signal.second)||!reader.Eof())throw std::runtime_error("Invalid source BATTLESTATUS body");
    auto operation=co_await ctx.battle_operations->Acquire();
    // Source Control sends this SM frame too (TControlSvr/TServer.cpp181).
    // A native Map or anonymous peer must not impersonate the internal timer.
    if(ctx.native_battles&&!authorized()) {
        peer->Wire()->Close();co_return;
    }
    const auto observed=static_cast<std::int64_t>(std::time(nullptr));
    if(ctx.native_battles) {
        const auto clock=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
            [repo=ctx.native_battles,observed]{return repo->Observe(observed);});
        (void)clock;
        if(!authorized())co_return;
    }
    // An operator broadcast does not alter m_battletime's timer fields in the
    // source. Keep that separate from the scheduler's own state transition.
    if(ctx.native_battles)Enqueue(ctx,signal);
    else co_await Publish(ctx,signal);
}
}
