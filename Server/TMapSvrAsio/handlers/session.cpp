#include "handlers.h"
#include "domain/connect.h"
#include "fourstory/db/co_offload.h"

#include "audit/audit_log.h"
#include "audit/event.h"
#include "services/channel_presence.h"
#include "services/char_state_store.h"
#include "services/client_senders.h"
#include "services/monster_chart.h"
#include "services/monster_registry.h"
#include "services/session_registry.h"
#include "services/session_validator.h"
#include "services/player_service.h"
#include "services/skill_cooldown.h"
#include "services/world_client.h"
#include "services/world_senders.h"
#include "wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace tmapsvr {


boost::asio::awaitable<void>
OnConnectReq(std::shared_ptr<tnetlib::AsioSession> sess,
             std::vector<std::byte>                body,
             const HandlerContext&                 ctx)
{
    using tnetlib::protocol::MessageId;

    wire::Reader r(body.data(), body.size());
    std::uint16_t version{}, port{};
    std::uint8_t channel{};
    std::uint32_t user{}, character{}, key{}, ip{};
    std::uint64_t checksum{};
    if (body.size() != 29 || !r.Read(version) || !r.Read(channel) ||
        !r.Read(user) || !r.Read(character) || !r.Read(key) || !r.Read(ip) ||
        !r.Read(port) || !r.Read(checksum)) {
        sess->Close(); co_return;
    }
    if (version != kClientVersion) {
        co_await sess->SendPacket(static_cast<std::uint16_t>(MessageId::CS_CONNECT_ACK),
            EncodeConnectAck(static_cast<std::uint8_t>(ConnectResult::InvalidVer), {}), true);
        co_return;
    }
    // The original server closes silently on checksum/repeated-handshake errors.
    if (checksum != ConnectChecksum(version, user, character, key) ||
        (ctx.session_reg && ctx.session_reg->Identity(sess.get()))) {
        sess->Close(); co_return;
    }

    ConnectResult result = ConnectResult::Internal;
    std::optional<MapSessionInfo> row;
    SessionIdentity requested{character,user,key,channel};
    requested.endpoint_ip=ip;requested.endpoint_port=port;
    if (ctx.validator && ctx.session_reg && ctx.world_client && ctx.world_client->IsRegistered()) {
        auto* validator = ctx.validator;
        try {
            row = co_await fourstory::db::CoOffloadIf(ctx.db_pool,
                [validator, user, key] { return validator->LookupSession(user, key); });
            result = ConnectResult::Ok;
        } catch (...) { result = ConnectResult::Internal; }
        if (!sess->IsOpen()) co_return;
        if (result == ConnectResult::Ok) {
            if (!row || row->bLocked || row->dwUserID != user || row->dwKEY != key ||
                row->dwCharID != character || character == 0)
                result = ConnectResult::InvalidChar;
            else if (row->bGroupID != ctx.expected_group || row->bChannel != channel)
                result = ConnectResult::NoChannel;
            else if (!ctx.session_reg->TryBind(requested, sess))
                result = ConnectResult::Duplicate;
        }
    }
    if (result==ConnectResult::Ok) {
        const auto identity=ctx.session_reg->Identity(sess.get());
        if (!identity) { sess->Close(); co_return; }
        const auto claim=identity->Claim(ctx.expected_group);
        try {
            auto* validator=ctx.validator;
            const auto candidate=*row; // the read-only check precedes reservation
            const auto acquired=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
                [validator,claim,candidate] { return validator->ClaimSession(claim,candidate); });
            if (!acquired || acquired->bLocked || acquired->dwCharID!=character ||
                acquired->dwUserID!=user || acquired->dwKEY!=key)
                result=ConnectResult::InvalidChar;
            else if (acquired->bGroupID!=ctx.expected_group || acquired->bChannel!=channel)
                result=ConnectResult::NoChannel;
            else if(!ctx.session_reg->SetRole(sess.get(),acquired->role))
                result=ConnectResult::Internal;
        } catch (...) { result=ConnectResult::Internal; }
        // The registry already owns the generation. Teardown releases even a
        // claim whose commit raced this close or whose acknowledgement was lost.
        if (!sess->IsOpen()) co_return;
    }
    if (ctx.audit) {
        audit::LoginAttemptEvent ev{};
        ev.hdr.corr = ctx.audit->NextCorrelation();
        ev.user_id = user; ev.char_id = character; ev.channel = channel;
        ev.version = version; ev.result = static_cast<std::uint8_t>(result);
        ctx.audit->Emit(ev);
    }
    if (result == ConnectResult::Ok) {
        // Only an attempted World announcement may later request World close.
        // Set before suspension: a reply/close can arrive while the write waits.
        if (!ctx.session_reg->SetWorldPresence(sess.get(), WorldPresence::Announced)) {
            sess->Close(); co_return;
        }
        // No client success here: MW_CONRESULT_REQ is the only authoritative ACK.
        if (co_await ctx.world_client->SendPacket(
            static_cast<std::uint16_t>(MessageId::MW_ADDCHAR_ACK),
            EncodeAddCharAck(character, key, ip, port, user))) co_return;
        ctx.session_reg->Transition(character, key, SessionPhase::Pending, SessionPhase::Rejected);
        result = ConnectResult::Internal;
    }
    spdlog::info("CS_CONNECT_REQ uid={} char={} result={}", user, character, static_cast<unsigned>(result));
    co_await sess->SendPacket(static_cast<std::uint16_t>(MessageId::CS_CONNECT_ACK),
        EncodeConnectAck(static_cast<std::uint8_t>(result), {}), true);
}

boost::asio::awaitable<void>
OnConReadyReq(std::shared_ptr<tnetlib::AsioSession> sess,
              std::vector<std::byte>                body,
              const HandlerContext&                 ctx)
{
    using tnetlib::protocol::MessageId;

    auto identity = ctx.session_reg ? ctx.session_reg->Identity(sess.get()) : std::nullopt;
    if(body.empty()&&identity&&identity->phase==SessionPhase::Ready)co_return;
    if(body.empty()&&identity&&identity->phase==SessionPhase::TransferOut&&ctx.player_service) {
        bool committed=false;
        try {auto* players=ctx.player_service;
            committed=co_await fourstory::db::CoOffloadIf(ctx.db_pool,[players,claim=identity->Claim(ctx.expected_group)]{return players->OutgoingTransferCommitted(claim);});
        }catch(...){sess->Close();co_return;}
        if(!committed||!ctx.session_reg->FinishTransfer(sess.get(),MapSessionRole::Replica,identity->authority_epoch+1)){sess->Close();co_return;}
        if(ctx.skill_cooldown)ctx.skill_cooldown->Forget(identity->char_id);
        if(auto s=ctx.char_state?ctx.char_state->Get(identity->char_id):std::nullopt) {
            // Retained source becomes the same partial observer as initial
            // replica admission. Its old graph is preserved in the journal.
            s->payload.reset();ctx.char_state->Store(identity->char_id,*s);
            if(sess->IsOpen()&&ctx.presence){ctx.presence->Bind(identity->char_id,identity->channel,sess);
                ctx.presence->UpdatePosition(identity->char_id,s->wMapID,{s->fPosX,s->fPosY,s->fPosZ});}
        }
        co_return;
    }
    if (!body.empty() || !identity || identity->phase != SessionPhase::Admitted) {
        sess->Close(); co_return;
    }
    const auto cid=identity->char_id;
    const auto snap=ctx.char_state?ctx.char_state->Get(cid):std::nullopt;
    if(!snap){sess->Close();co_return;}
    if (ctx.validator) {
        const auto claim=identity->Claim(ctx.expected_group);auto* validator=ctx.validator;
        try { co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,[validator,claim,&snap] { validator->MarkReady(claim,*snap); }); }
        catch (...) { sess->Close(); co_return; }
    }
    // Even if the socket closed during the commit, teardown now owns a complete
    // snapshot and must save/release the committed ready claim. Do not leave it
    // labelled Admitted in memory and strand an otherwise clean account.
    if(!ctx.session_reg->Transition(cid,identity->key,SessionPhase::Admitted,SessionPhase::Ready)){
        sess->Close();co_return;
    }
    if(!sess->IsOpen())co_return;
    if (ctx.presence) ctx.presence->Bind(cid, identity->channel, sess);

    // Seed the live presence position from the loaded snapshot so AOI and
    // the monster-chase AI have a real location before the first
    // CS_MOVE_REQ (presence.pos otherwise sits at the origin until a move).
    if (ctx.presence)
        ctx.presence->UpdatePosition(cid, snap->wMapID,
            Position{ snap->fPosX, snap->fPosY, snap->fPosZ });

    spdlog::info("CS_CONREADY_REQ char={} name='{}' map={} — connection ready "
                 "(source CHARINFO already sent on native primary)", cid, snap->szNAME, snap->wMapID);

    const bool primary=identity->role==MapSessionRole::Primary;
    if(primary&&!snap->payload) {
        const auto ack = EncodeCharInfoAck(*snap, FormatServerClock());
        co_await sess->SendPacket(
            static_cast<std::uint16_t>(MessageId::CS_CHARINFO_ACK), ack);
    }

    // Enter-map AOI exchange: show the newcomer everyone already on its
    // channel, and announce the newcomer to them. The presence visitor
    // is synchronous, so collect the (snapshot, session) pairs first and
    // co_await the sends afterwards. Monsters + NPCs join this flood once
    // the spawn / NPC-visibility layer lands.
    //
    // Positions come from each char's snapshot (its loaded spawn point):
    // ChannelPresence.pos is only seeded on the first CS_MOVE_REQ, so for
    // a just-entered crowd the snapshot is the authoritative location.
    // Live presence-position tracking is a follow-up.
    if (ctx.presence && ctx.char_state)
    {
        const auto me = ctx.presence->FindEntry(cid);
        if (me)
        {
            struct Nearby
            {
                CharSnapshot                          snap;
                std::shared_ptr<tnetlib::AsioSession> sess;
                bool primary{};
            };
            std::vector<Nearby> nearby;
            ctx.presence->ForEachInChannel(me->channel, cid,
                [&](const ChannelPresenceEntry& e,
                    std::shared_ptr<tnetlib::AsioSession> osess)
                {
                    const auto other=ctx.session_reg->Identity(osess.get());
                    if (auto osnap = ctx.char_state->Get(e.char_id);osnap&&other)
                        nearby.push_back({ std::move(*osnap), std::move(osess),other->role==MapSessionRole::Primary });
                });

            // Faction tint (legacy CanFight / TNCOLOR) is PvP gameplay —
            // default friendly until the combat layer lands.
            constexpr std::uint8_t kColorFriendly = 0;
            const Position my_pos{ snap->fPosX, snap->fPosY, snap->fPosZ };
            const auto my_enter = primary?
                EncodeEnterAck(*snap, my_pos, kColorFriendly, /*new_member=*/1):std::vector<std::byte>{};

            for (auto& n : nearby)
            {
                const Position their_pos{ n.snap.fPosX, n.snap.fPosY,
                                          n.snap.fPosZ };
                // Original TCell::EnterPlayer exposes only primary actors;
                // a replica is an observer, not another full character graph.
                if(n.primary) {
                    const auto their = EncodeEnterAck(n.snap, their_pos, kColorFriendly,/*new_member=*/0);
                    co_await sess->SendPacket(static_cast<std::uint16_t>(MessageId::CS_ENTER_ACK), their);
                }
                if (primary&&n.sess)
                    co_await n.sess->SendPacket(           // they see me arrive
                        static_cast<std::uint16_t>(MessageId::CS_ENTER_ACK),
                        my_enter);
            }

            if (!nearby.empty())
                spdlog::info("CS_CONREADY char={} — CS_ENTER_ACK exchanged with "
                             "{} nearby players", cid, nearby.size());

            // Monster half of the enter flood: every monster on the
            // player's channel + map. The static SpawnManager places the
            // standing population on channel 0 at boot (see main.cpp), so
            // a channel-0 player sees them; per-channel instances + the
            // respawn / AI tick are the next increments. new_member = 0
            // (listing the standing crowd, not a fresh spawn); the faction
            // tint stays hostile-default until the combat layer.
            if (ctx.monster_registry)
            {
                const auto mons =
                    ctx.monster_registry->ListInMap(me->channel, snap->wMapID);
                for (const auto& mon : mons)
                {
                    std::uint8_t level = 0;
                    if (ctx.monster_chart)
                    {
                        if (const auto t = ctx.monster_chart->Find(mon.wTemplateID))
                            level = t->bLevel;
                    }
                    const auto madd = EncodeAddMonAck(
                        mon, level, /*country=*/0, /*color=*/0,
                        /*new_member=*/0);
                    co_await sess->SendPacket(
                        static_cast<std::uint16_t>(MessageId::CS_ADDMON_ACK),
                        madd);
                }
                if (!mons.empty())
                    spdlog::info("CS_CONREADY char={} — CS_ADDMON for {} "
                                 "monster(s) on ch={} map={}",
                        cid, mons.size(), me->channel, snap->wMapID);
            }
        }
    }
}

} // namespace tmapsvr
