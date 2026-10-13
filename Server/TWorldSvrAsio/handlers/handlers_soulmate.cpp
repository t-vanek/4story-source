#include "handlers.h"
#include "../senders/senders.h"
#include "../services/soulmate_constants.h"
#include "../wire_codec.h"
#include "MessageId.h"

#include "fourstory/db/co_offload.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

namespace tworldsvr::handlers {

namespace {

bool CurrentWriter(const HandlerContext& ctx,const std::shared_ptr<PeerSession>& peer,
                   const std::shared_ptr<TChar>& ch,std::uint32_t key) {
    if(!ch||!peer||!peer->Wire()->IsOpen()||!ctx.peers||ctx.peers->Find(peer->Wid())!=peer)return false;
    std::lock_guard guard(ch->lock);const auto server=static_cast<std::uint8_t>(peer->Wid());
    const auto con=std::find_if(ch->cons.begin(),ch->cons.end(),[server](const auto& c){return c.server_id==server;});
    return peer->Wid()==(0x0400U|server)&&ch->key==key&&!ch->closing&&!ch->logout&&ch->main_server_id==server&&
           con!=ch->cons.end()&&con->ready&&con->valid;
}
// RegSoulmate writes only the initiator. Replace its old inbound projection,
// while preserving the target's independent own pairing (source RegSoulmate).
struct Mate;
void ProjectPair(const HandlerContext& ctx,TChar& self,const Mate& m);

// One char's describe-fields needed to fill the partner's TSoulmate.
struct Mate
{
    std::uint32_t id = 0, region = 0;
    std::uint8_t  level = 0, klass = 0;
    std::string   name;
    std::uint32_t key=0;
    std::uint8_t server=0;
};

Mate SnapshotMate(TChar& c)   // caller holds c.lock
{
    return Mate{c.char_id, c.region, c.level, c.klass, c.name,c.key,c.main_server_id};
}

// Set `self`'s soulmate to `m` (caller holds self.lock).
void SetSoulmate(TChar& self, const Mate& m)
{
    self.soulmate = TSoulmate{m.id, m.name, m.level, m.klass, true, m.region};
    self.soul_silence = 0;   // legacy TWorldSvr.cpp:4333
}

void ProjectPair(const HandlerContext& ctx,TChar& self,const Mate& m) {
    std::uint32_t old=0;Mate own;
    {std::lock_guard guard(self.lock);old=self.soulmate.target;own=SnapshotMate(self);SetSoulmate(self,m);}
    if(auto previous=ctx.chars->Find(old)) {std::lock_guard guard(previous->lock);previous->inbound_soulmates.erase(own.id);}
    bool online=false;
    if(auto target=ctx.chars->Find(m.id)) {std::lock_guard guard(target->lock);
        online=!target->closing&&!target->logout;
        if(online)target->inbound_soulmates[own.id]=TSoulmate{own.id,own.name,own.level,own.klass,true,own.region};}
    {std::lock_guard guard(self.lock);if(self.soulmate.target==m.id){self.soulmate.connected=online;self.soulmate.region=online?m.region:0;}}
}

} // namespace

boost::asio::awaitable<void>
RunSoulmateMaintenance(const HandlerContext& ctx,std::int64_t& cursor)
{
    if(!ctx.soulmate_offers||!ctx.chars||!ctx.peers||!ctx.friend_repo)co_return;
    auto operation=co_await ctx.friend_operations->Acquire();
    const auto committed=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
        [offers=ctx.soulmate_offers,after=cursor]{return offers->Committed(after);});
    for(const auto& offer:committed) {
        cursor=offer.offer_id;
        auto self=ctx.chars->Find(offer.character);
        auto main=ctx.peers->Find(static_cast<std::uint16_t>(0x0400U|offer.server));
        if(!CurrentWriter(ctx,main,self,offer.key))continue;
        Mate mate{offer.target,offer.region,offer.level,offer.klass,offer.name,offer.target_key,offer.target_server};
        if(auto target=ctx.chars->Find(offer.target)) {
            std::lock_guard guard(target->lock);
            if(!target->closing&&!target->logout&&!target->db_loading)mate=SnapshotMate(*target);
        }
        // Repeated receipt observations are cache reconciliation only, never
        // another purchase or client success. SQL filters an END/newer REG.
        ProjectPair(ctx,*self,mate);
    }
    if(committed.size()<128)cursor=0;

    // Source CheckSoulmateEnd also runs during initial own/incoming hydration.
    // Native entry is claimed/loaded until CHARINFO+ROUTE complete; wait for
    // that exact ready primary before stamping any database relationship.
    auto ids=ctx.chars->SnapshotIds();std::sort(ids.begin(),ids.end());
    for(const auto id:ids) {
        auto owner=ctx.chars->Find(id);if(!owner)continue;
        std::uint32_t key=0,target=0;std::uint8_t server=0,level=0,mate_level=0;
        {std::lock_guard guard(owner->lock);
         key=owner->key;server=owner->main_server_id;target=owner->soulmate.target;
         level=owner->level;mate_level=owner->soulmate.level;}
        if(!target)continue;
        auto main=ctx.peers->Find(static_cast<std::uint16_t>(0x0400U|server));
        if(!CurrentWriter(ctx,main,owner,key))continue;
        if(auto partner=ctx.chars->Find(target)) {
            std::lock_guard guard(partner->lock);
            if(!partner->db_loading&&!partner->closing&&!partner->logout)mate_level=partner->level;
        }
        if(std::abs(int(level)-int(mate_level))<=soulmate::kLevelWindow)continue;
        const auto ended=static_cast<std::uint32_t>(std::time(nullptr));
        const auto saved=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
            [repo=ctx.friend_repo,id,target,ended,key,server]
            {return repo->EndSoulmateExact(id,target,ended,{key,server});});
        if(!saved||ctx.chars->Find(id)!=owner||!CurrentWriter(ctx,main,owner,key))continue;
        {std::lock_guard guard(owner->lock);
         if(owner->soulmate.target!=target)continue;
         owner->soulmate=TSoulmate{};owner->soul_silence=ended;}
        if(auto partner=ctx.chars->Find(target)) {std::lock_guard guard(partner->lock);partner->inbound_soulmates.erase(id);}
        co_await senders::SendMwSoulmateEndReq(main,id,key,soulmate::kSuccess,ended);
    }
}

boost::asio::awaitable<void>
NotifySoulmateOnLogout(const HandlerContext& ctx, std::shared_ptr<TChar> who)
{
    if (!ctx.chars || !who) co_return;
    std::uint32_t who_id = 0, partner = 0;
    bool          connected = false;
    {
        std::lock_guard g(who->lock);
        who_id    = who->char_id;
        partner   = who->soulmate.target;
        connected = who->soulmate.connected;
    }
    if(partner&&connected)if(auto p=ctx.chars->Find(partner)) {
        std::lock_guard guard(p->lock);auto it=p->inbound_soulmates.find(who_id);
        if(it!=p->inbound_soulmates.end()){it->second.connected=false;it->second.region=0;}
    }
    std::vector<std::uint32_t> incoming;
    {std::lock_guard guard(who->lock);for(const auto& [owner,mate]:who->inbound_soulmates)incoming.push_back(owner);}
    for(auto owner:incoming)if(auto p=ctx.chars->Find(owner)) {
        std::lock_guard guard(p->lock);if(p->soulmate.target==who_id){p->soulmate.connected=false;p->soulmate.region=0;}
    }
    co_return;   // legacy LeaveSoulmate sends no packet
}

boost::asio::awaitable<void>
OnSoulmateSearchAck(std::shared_ptr<PeerSession> peer,
                    std::vector<std::byte>       body,
                    const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars) { spdlog::warn("OnSoulmateSearchAck[{}]: no chars", ip);
        co_return; }

    auto operation=co_await ctx.friend_operations->Acquire();
    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::uint8_t  min_level = 0, npc_inven = 0, npc_item = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(min_level) ||
        !r.Read(npc_inven) || !r.Read(npc_item)||!r.Eof())
    {
        spdlog::warn("OnSoulmateSearchAck[{}]: short body", ip);
        co_return;
    }

    auto searcher = ctx.chars->Find(char_id);
    if (!searcher) co_return;
    if(!CurrentWriter(ctx,peer,searcher,key))co_return;
    std::uint8_t  s_country = 0, s_level = 0, s_realsex = 0, s_sex = 0;
    {
        std::lock_guard g(searcher->lock);
        if (searcher->key != key) co_return;
        s_country = searcher->country; s_level = searcher->level;
        s_realsex = searcher->real_sex; s_sex = searcher->sex;
    }

    struct Cand { Mate mate; std::uint8_t realsex = 0, sex = 0;
                  bool has_sm = false; };
    std::vector<Cand> cands;
    std::uint8_t best = min_level;
    auto candidate_ids=ctx.chars->SnapshotIds();
    std::sort(candidate_ids.begin(),candidate_ids.end()); // Original DWORD-keyed map order.
    for (auto id : candidate_ids)
    {
        if (id == char_id) continue;
        auto c = ctx.chars->Find(id);
        if (!c) continue;
        Cand cand;
        std::uint8_t c_country = 0, c_level = 0;
        {
            std::lock_guard g(c->lock);
            if(ctx.soulmate_offers&&(c->closing||c->logout||c->db_loading))continue;
            c_country = c->country; c_level = c->level;
            cand.realsex = c->real_sex; cand.sex = c->sex;
            cand.has_sm = (c->soulmate.target != 0);
            cand.mate = SnapshotMate(*c);
        }
        if(ctx.soulmate_offers) {
            auto candidate_peer=ctx.peers->Find(static_cast<std::uint16_t>(0x0400U|cand.mate.server));
            if(!CurrentWriter(ctx,candidate_peer,c,cand.mate.key))continue;
        }
        if (c_country != s_country) continue;
        if (std::abs(int(s_level) - int(c_level)) >= soulmate::kLevelWindow)
            continue;
        if (best > c_level) { cands.clear(); cands.push_back(cand);
                              best = c_level; }
        else if (best == c_level) cands.push_back(cand);
    }

    if (cands.empty())
    {
        const auto response=senders::EncodeMwSoulmateSearchReq(char_id,key,soulmate::kNotFound,0,{},0,npc_inven,npc_item);
        if(ctx.soulmate_offers) {
            std::vector<std::byte> request;wire::WritePOD(request,npc_inven);wire::WritePOD(request,npc_item);
            const auto saved=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
                [offers=ctx.soulmate_offers,char_id,key,server=static_cast<std::uint8_t>(peer->Wid()),request,response]
                {return offers->Prepare(char_id,{key,server},tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_SOULMATESEARCH_REQ),request,
                    tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::MW_SOULMATESEARCH_REQ),response,0,{},0,0,0);});
            if(!saved||ctx.chars->Find(char_id)!=searcher||!CurrentWriter(ctx,peer,searcher,key))co_return;
        }
        co_await peer->Wire()->SendPacket(tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::MW_SOULMATESEARCH_REQ),response);
        co_return;
    }

    auto filter = [&](auto pred) {
        if (cands.size() <= 1) return;
        std::vector<Cand> t;
        for (const auto& c : cands) if (pred(c)) t.push_back(c);
        if (!t.empty()) cands = std::move(t);
    };
    filter([&](const Cand& c) { return c.realsex != s_realsex; });
    filter([&](const Cand& c) { return !c.has_sm; });
    filter([&](const Cand& c) { return c.sex != s_sex; });

    const Mate match = cands.front().mate;

    if(ctx.soulmate_offers) {
        std::vector<std::byte> request;wire::WritePOD(request,npc_inven);wire::WritePOD(request,npc_item);
        const auto response=senders::EncodeMwSoulmateSearchReq(char_id,key,soulmate::kSuccess,match.id,match.name,match.region,npc_inven,npc_item);
        const auto saved=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
            [offers=ctx.soulmate_offers,char_id,key,server=static_cast<std::uint8_t>(peer->Wid()),request,response,match]
            {return offers->Prepare(char_id,{key,server,match.key,match.server},
                tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_SOULMATESEARCH_REQ),request,
                tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::MW_SOULMATESEARCH_REQ),response,
                match.id,match.name,match.region,match.level,match.klass);});
        if(!saved||ctx.chars->Find(char_id)!=searcher||!CurrentWriter(ctx,peer,searcher,key))co_return;
        co_await peer->Wire()->SendPacket(tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::MW_SOULMATESEARCH_REQ),response);
        co_return;
    } else if(ctx.friend_repo) {
        const auto saved=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
            [repo=ctx.friend_repo,char_id,mid=match.id,key,peer]
            {return repo->RegSoulmate(char_id,mid,{key,static_cast<std::uint8_t>(peer->Wid())});});
        if(!saved)co_return;
    }
    if(ctx.chars->Find(char_id)!=searcher||!CurrentWriter(ctx,peer,searcher,key))co_return;
    if(ctx.chars->Find(match.id))ProjectPair(ctx,*searcher,match);else co_return;

    co_await senders::SendMwSoulmateSearchReq(peer, char_id, key,
        soulmate::kSuccess, match.id, match.name, match.region, npc_inven,
        npc_item);
    spdlog::info("OnSoulmateSearchAck[{}]: char_id={} matched {} ('{}')",
        ip, char_id, match.id, match.name);
    co_return;
}

boost::asio::awaitable<void>
OnSoulmateRegAck(std::shared_ptr<PeerSession> peer,
                 std::vector<std::byte>       body,
                 const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars) { spdlog::warn("OnSoulmateRegAck[{}]: no chars", ip);
        co_return; }

    auto operation=co_await ctx.friend_operations->Acquire();
    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::string   name;
    std::uint8_t  reg = 0, npc_inven = 0, npc_item = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.ReadString(name) ||
        !r.Read(reg) || !r.Read(npc_inven) || !r.Read(npc_item)||!r.Eof())
    {
        spdlog::warn("OnSoulmateRegAck[{}]: short body", ip);
        co_return;
    }

    auto self = ctx.chars->Find(char_id);
    if (!self) co_return;
    if(!CurrentWriter(ctx,peer,self,key))co_return;
    std::uint8_t s_country = 0, s_level = 0;
    {
        std::lock_guard g(self->lock);
        if (self->key != key) co_return;
        s_country = self->country; s_level = self->level;
    }

    auto reply = [&](std::uint8_t result, std::uint32_t sm,
                     const std::string& sname, std::uint32_t region,std::uint32_t target_key=0,
                     std::uint8_t target_server=0,std::uint8_t target_level=0,std::uint8_t target_class=0)
        -> boost::asio::awaitable<void> {
        const auto response=senders::EncodeMwSoulmateRegReq(char_id,key,result,reg,npc_inven,npc_item,sm,sname,region);
        if(reg&&ctx.soulmate_offers) {
            std::vector<std::byte> request;wire::WriteString(request,name);wire::WritePOD(request,npc_inven);wire::WritePOD(request,npc_item);
            const auto saved=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
                [offers=ctx.soulmate_offers,char_id,key,server=static_cast<std::uint8_t>(peer->Wid()),target_key,target_server,
                 request,response,sm,sname,region,target_level,target_class]
                {return offers->Prepare(char_id,{key,server,target_key,target_server},
                    tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_SOULMATEREG_REQ),request,
                    tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::MW_SOULMATEREG_REQ),response,
                    sm,sname,region,target_level,target_class);});
            if(!saved)co_return;
        }
        if(ctx.chars->Find(char_id)!=self||!CurrentWriter(ctx,peer,self,key))co_return;
        co_await peer->Wire()->SendPacket(tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::MW_SOULMATEREG_REQ),response);
    };

    auto target = ctx.chars->FindByName(name);
    if (!target)
    {
        co_await reply(soulmate::kNotFound, 0, std::string{}, 0);
        co_return;
    }
    std::uint8_t t_country = 0, t_level = 0;
    Mate         t_mate;
    {
        std::lock_guard g(target->lock);
        t_country = target->country; t_level = target->level;
        t_mate = SnapshotMate(*target);
    }
    if(ctx.soulmate_offers) {
        auto target_peer=ctx.peers->Find(static_cast<std::uint16_t>(0x0400U|t_mate.server));
        if(!CurrentWriter(ctx,target_peer,target,t_mate.key)) {
            co_await reply(soulmate::kNotFound,0,{},0);co_return;
        }
    }
    if (t_country != s_country ||
        std::abs(int(t_level) - int(s_level)) > soulmate::kLevelWindow)
    {
        co_await reply(soulmate::kFail, 0, std::string{}, 0);
        co_return;
    }

    if (reg&&!ctx.soulmate_offers)
    {
        if(ctx.friend_repo) {
            const auto saved=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
                [repo=ctx.friend_repo,char_id,tid=t_mate.id,key,peer]
                {return repo->RegSoulmate(char_id,tid,{key,static_cast<std::uint8_t>(peer->Wid())});});
            if(!saved)co_return;
        }
        if(ctx.chars->Find(char_id)!=self||ctx.chars->Find(t_mate.id)!=target||!CurrentWriter(ctx,peer,self,key))co_return;
        ProjectPair(ctx,*self,t_mate);
    }
    co_await reply(soulmate::kSuccess,t_mate.id,t_mate.name,t_mate.region,t_mate.key,t_mate.server,t_mate.level,t_mate.klass);
    co_return;
}

boost::asio::awaitable<void>
OnSoulmateEndAck(std::shared_ptr<PeerSession> peer,
                 std::vector<std::byte>       body,
                 const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars) { spdlog::warn("OnSoulmateEndAck[{}]: no chars", ip);
        co_return; }

    auto operation=co_await ctx.friend_operations->Acquire();
    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(char_id) || !r.Read(key)||!r.Eof())
    {
        spdlog::warn("OnSoulmateEndAck[{}]: short body", ip);
        co_return;
    }

    auto self = ctx.chars->Find(char_id);
    if (!self) co_return;
    if(!CurrentWriter(ctx,peer,self,key))co_return;
    std::uint32_t partner = 0;
    {
        std::lock_guard g(self->lock);
        if (self->key != key) co_return;
        partner = self->soulmate.target;
    }
    if (partner == 0)
    {
        co_await senders::SendMwSoulmateEndReq(peer, char_id, key,
            soulmate::kFail, 0);
        co_return;
    }

    const auto end_time=static_cast<std::uint32_t>(std::time(nullptr));
    if(ctx.friend_repo) {
        const auto saved=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
            [repo=ctx.friend_repo,char_id,partner,end_time,key,peer]
            {return repo->EndSoulmateExact(char_id,partner,end_time,{key,static_cast<std::uint8_t>(peer->Wid())});});
        if(!saved)co_return;
    }
    if(ctx.chars->Find(char_id)!=self||!CurrentWriter(ctx,peer,self,key))co_return;
    {std::lock_guard guard(self->lock);self->soulmate=TSoulmate{};self->soul_silence=end_time;}
    if(auto p=ctx.chars->Find(partner)){std::lock_guard guard(p->lock);p->inbound_soulmates.erase(char_id);}
    co_await senders::SendMwSoulmateEndReq(peer, char_id, key,
        soulmate::kSuccess, end_time);
    spdlog::info("OnSoulmateEndAck[{}]: char_id={} dissolved pairing with {}",
        ip, char_id, partner);
    co_return;
}

} // namespace tworldsvr::handlers
