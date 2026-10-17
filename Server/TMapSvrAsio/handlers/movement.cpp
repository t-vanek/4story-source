// Movement handler — CS_MOVE_REQ decodes the 26-byte position update,
// updates actual native cell membership and visibility, then publishes the
// 27-byte CS_MOVE_ACK to source GetNeighbor recipients. The development adapter
// retains its older channel-wide presence path.
//
// Source: CSHandler.cpp:439 (OnCS_MOVE_REQ), TPlayer::MoveGhost/CanHost
// and CSSender.cpp:599 (SendCS_MOVE_ACK). Native PC-only movement includes
// speed/operator, HP0 keys, ghost/body separation and local host eligibility.
// Populated monster event/host consumers, hide erasure and position quests
// remain incomplete; the native process admission barrier stays in force.

#include "handlers.h"

#include "services/channel_presence.h"
#include "services/char_state_store.h"
#include "services/player_service.h"
#include "services/native_cell_runtime.h"
#include "services/session_registry.h"
#include "services/session_validator.h"
#include "services/world_client.h"
#include "fourstory/db/co_offload.h"
#include "wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

namespace tmapsvr {

namespace {

// CS_MOVE_ACK body — 27 bytes broadcast to other channel members
// after a CS_MOVE_REQ. The full field set lets each client
// interpolate the moving player's position, facing, action, and
// speed.
std::vector<std::byte> EncodeMoveAck(std::uint32_t dwCharID,
                                     float fPosX, float fPosY, float fPosZ,
                                     std::uint16_t wPitch, std::uint16_t wDIR,
                                     std::uint8_t bMouseDIR, std::uint8_t bKeyDIR,
                                     std::uint8_t bAction, float fSpeed)
{
    std::vector<std::byte> body;
    body.reserve(27);
    wire::WritePOD<std::uint32_t>(body, dwCharID);
    wire::WritePOD<float>        (body, fPosX);
    wire::WritePOD<float>        (body, fPosY);
    wire::WritePOD<float>        (body, fPosZ);
    wire::WritePOD<std::uint16_t>(body, wPitch);
    wire::WritePOD<std::uint16_t>(body, wDIR);
    wire::WritePOD<std::uint8_t> (body, bMouseDIR);
    wire::WritePOD<std::uint8_t> (body, bKeyDIR);
    wire::WritePOD<std::uint8_t> (body, bAction);
    wire::WritePOD<float>        (body, fSpeed);
    return body;
}

} // namespace

boost::asio::awaitable<void>
OnMoveReq(std::shared_ptr<tnetlib::AsioSession> sess,
          std::vector<std::byte>                body,
          const HandlerContext&                 ctx)
{
    using tnetlib::protocol::MessageId;

    // Wire layout from legacy CSHandler.cpp::OnCS_MOVE_REQ (26 bytes):
    //   WORD  wMapID
    //   FLOAT fPosX  fPosY  fPosZ
    //   WORD  wPitch wDIR
    //   BYTE  bMouseDIR bKeyDIR bAction bGhost
    //   FLOAT fSpeed
    // Native processing below uses the admitted map rather than the source's
    // unused wMapID input. The development adapter retains its older path.
    wire::Reader r(body.data(), body.size());
    std::uint16_t wMapID    = 0;
    float fPosX = 0.f, fPosY = 0.f, fPosZ = 0.f;
    std::uint16_t wPitch    = 0;
    std::uint16_t wDIR      = 0;
    std::uint8_t  bMouseDIR = 0, bKeyDIR = 0, bAction = 0, bGhost = 0;
    float fSpeed = 0.f;

    if (!r.Read(wMapID) ||
        !r.Read(fPosX)  || !r.Read(fPosY) || !r.Read(fPosZ) ||
        !r.Read(wPitch) || !r.Read(wDIR)  ||
        !r.Read(bMouseDIR) || !r.Read(bKeyDIR) || !r.Read(bAction) ||
        !r.Read(bGhost) || !r.Read(fSpeed))
    {
        spdlog::warn("CS_MOVE_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }

    if(!r.Eof()||!std::isfinite(fPosX)||!std::isfinite(fPosY)||!std::isfinite(fPosZ)||!std::isfinite(fSpeed))co_return;

    if (!ctx.session_reg || !ctx.presence)
    {
        spdlog::debug("CS_MOVE_REQ: presence/session_reg not configured — "
                      "dropping move (no broadcast destination)");
        co_return;
    }

    const auto cid_opt = ctx.session_reg->FindCharIdBySession(sess.get());
    if (!cid_opt)
    {
        spdlog::debug("CS_MOVE_REQ from unbound session — dropping "
                      "(client must complete CS_CONNECT_REQ first)");
        co_return;
    }
    const std::uint32_t dwCharID = *cid_opt;

    const auto entry = ctx.presence->FindEntry(dwCharID);
    if (!entry)
    {
        spdlog::debug("CS_MOVE_REQ char={} not in presence map — dropping",
            dwCharID);
        co_return;
    }
    if(ctx.player_service&&ctx.player_service->HasNativeCellAuthority()) {
        const auto identity=ctx.session_reg->Identity(sess.get());
        if(!identity||identity->phase!=SessionPhase::Ready||!ctx.char_state)co_return;
        const auto current_request=[&] {
            const auto current=ctx.session_reg->Identity(sess.get());
            return sess->IsOpen()&&current&&current->phase==SessionPhase::Ready&&
                SameNativeCellPlayer(current->Claim(ctx.expected_group),identity->Claim(ctx.expected_group));
        };
        // Source reads wMapID but acts on m_pMAP. Never move maps from that
        // client field; routing and membership use the actual admitted actor.
        const auto map=entry->map_id;
        if(fSpeed>3.40f) {
            bool allowed=false;
            try {
                auto* players=ctx.player_service;
                allowed=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
                    [players,claim=identity->Claim(ctx.expected_group),fSpeed] {
                        return players->AuthorizeNativeMovementSpeed(claim,fSpeed);
                    });
            } catch(...) {sess->Close();co_return;}
            if(!current_request())co_return;
            if(!allowed){sess->Close();co_return;}
        }
        NativePlayerMovePlan plan;
        try {
            const auto snapshot=ctx.char_state->Get(dwCharID);
            if(!snapshot||snapshot->wMapID!=map)throw std::runtime_error("Native MOVE lost actual map state");
            plan=ctx.player_service->PlanNativePlayerMove(identity->Claim(ctx.expected_group),*snapshot,
                NativePlayerMoveInput{fPosX,fPosY,fPosZ,fSpeed,wPitch,wDIR,bMouseDIR,bKeyDIR,bAction,bGhost});
        } catch(...) {sess->Close();co_return;}
        const auto after=NativeCellAt(plan.x,plan.z);
        if(!after){sess->Close();co_return;}
        if(!plan.early_return&&plan.origin_cell!=*after&&identity->role==MapSessionRole::Primary) {
            if(!ctx.validator||!ctx.world_client){sess->Close();co_return;}
            std::optional<std::vector<std::uint8_t>> ids;
            try {
                auto* validator=ctx.validator;
                ids=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
                    [validator,claim=identity->Claim(ctx.expected_group),map,x=plan.x,z=plan.z] {
                        return validator->MovementServers(claim,map,x,z);
                    });
            } catch(...) {sess->Close();co_return;}
            if(!current_request())co_return;
            if(!ids){sess->Close();co_return;}
            std::vector<std::byte> check;
            wire::WritePOD(check,dwCharID);wire::WritePOD(check,identity->key);wire::WritePOD(check,identity->channel);wire::WritePOD(check,map);
            wire::WritePOD(check,plan.x);wire::WritePOD(check,plan.y);wire::WritePOD(check,plan.z);
            wire::WritePOD<std::uint8_t>(check,static_cast<std::uint8_t>(ids->size()));for(auto id:*ids)wire::WritePOD(check,id);
            if(!co_await ctx.world_client->SendPacket(static_cast<std::uint16_t>(MessageId::MW_CHECKCONNECT_ACK),std::move(check))) {
                sess->Close();co_return;
            }
            if(!current_request())co_return;
        }
        // Apply fences the preparation's local revision as well as its exact
        // socket/role/epoch. No state or packet publication suspends below.
        try {
            const auto notices=ctx.player_service->ApplyNativePlayerMove(plan);
            ctx.char_state->Update(dwCharID,[&](CharSnapshot& s) {
                s.wDIR=plan.direction;s.fPosX=plan.x;s.fPosY=plan.y;s.fPosZ=plan.z;s.movement=plan.movement;
            });
            if(plan.early_return)co_return; // source updates speed/ghost/tick, but no body/MOVE output
            ctx.presence->UpdatePosition(dwCharID,map,{plan.x,plan.y,plan.z});
            const auto ack=EncodeMoveAck(dwCharID,plan.x,plan.y,plan.z,plan.movement.pitch,plan.direction,
                plan.movement.mouse,plan.movement.key,plan.movement.action,plan.movement.speed);
            PublishNativeCellNotices(ctx,notices,ack);
        } catch(...) {sess->Close();}
        co_return;
    }
    // The older development adapter retains its explicit map header check.
    if(entry->map_id!=wMapID)co_return;
    const std::uint8_t mover_channel = entry->channel;
    if(ctx.char_state)ctx.char_state->Update(dwCharID,[&](CharSnapshot& s){
        s.wDIR=wDIR;s.fPosX=fPosX;s.fPosY=fPosY;s.fPosZ=fPosZ;
    });

    // Persist the new position so the next ForEachInChannel snapshot
    // (or a later F8/F9 spatial query) reads the up-to-date value.
    ctx.presence->UpdatePosition(dwCharID, wMapID, Position{fPosX, fPosY, fPosZ});
    // The original broadcasts movement only from m_bMain. Replicas track their
    // client's position for local visibility, but cannot duplicate that actor.
    if(const auto identity=ctx.session_reg->Identity(sess.get());identity&&identity->role==MapSessionRole::Replica)co_return;

    const auto identity=ctx.session_reg->Identity(sess.get());
    const auto native=ctx.char_state?ctx.char_state->Get(dwCharID):std::nullopt;
    if(identity&&native&&native->payload&&ctx.validator&&ctx.world_client&&
       (std::floor(entry->pos.x/64)!=std::floor(fPosX/64)||std::floor(entry->pos.z/64)!=std::floor(fPosZ/64))) {
        std::optional<std::vector<std::uint8_t>> ids;
        try {auto* validator=ctx.validator;
            ids=co_await fourstory::db::CoOffloadIf(ctx.db_pool,[validator,claim=identity->Claim(ctx.expected_group),wMapID,fPosX,fPosZ]{
                return validator->MovementServers(claim,wMapID,fPosX,fPosZ);
            });
        }catch(...){sess->Close();co_return;}
        const auto current=ctx.session_reg->Identity(sess.get());
        if(!current||current->phase!=SessionPhase::Ready||current->authority_epoch!=identity->authority_epoch||!sess->IsOpen())co_return;
        if(!ids){sess->Close();co_return;}
        std::vector<std::byte> check;
        wire::WritePOD(check,dwCharID);wire::WritePOD(check,identity->key);wire::WritePOD(check,identity->channel);wire::WritePOD(check,wMapID);
        wire::WritePOD(check,fPosX);wire::WritePOD(check,fPosY);wire::WritePOD(check,fPosZ);
        wire::WritePOD<std::uint8_t>(check,static_cast<std::uint8_t>(ids->size()));for(auto id:*ids)wire::WritePOD(check,id);
        if(!co_await ctx.world_client->SendPacket(static_cast<std::uint16_t>(MessageId::MW_CHECKCONNECT_ACK),std::move(check))){sess->Close();co_return;}
    }

    // Collect every other in-channel session, then broadcast on the
    // same coroutine. The visit snapshot inside InMemoryChannelPresence
    // is done under the lock so the visitor itself runs lock-free —
    // safe to co_await SendPacket here without the mutex held.
    std::vector<std::shared_ptr<tnetlib::AsioSession>> recipients;
    ctx.presence->ForEachInChannel(mover_channel, dwCharID,
        [&recipients](const ChannelPresenceEntry&,
                      std::shared_ptr<tnetlib::AsioSession> sp) {
            recipients.push_back(std::move(sp));
        });

    spdlog::debug("CS_MOVE_REQ char={} ch={} map={} pos=({:.1f},{:.1f},{:.1f}) "
                  "-> broadcast {} peer(s)",
        dwCharID, mover_channel, wMapID, fPosX, fPosY, fPosZ,
        recipients.size());

    auto ack = EncodeMoveAck(dwCharID, fPosX, fPosY, fPosZ,
                             wPitch, wDIR, bMouseDIR, bKeyDIR, bAction,
                             fSpeed);
    for (auto& peer : recipients)
    {
        co_await peer->SendPacket(
            static_cast<std::uint16_t>(MessageId::CS_MOVE_ACK),
            std::span<const std::byte>(ack.data(), ack.size()));
    }
}

} // namespace tmapsvr
