// Movement handler — CS_MOVE_REQ decodes the 26-byte position update,
// persists it into the per-channel presence map, and fan-outs a
// 27-byte CS_MOVE_ACK to every other session sharing the channel
// (legacy flat-list AOI; the spatial grid lands with F13).
//
// Legacy parity: CSHandler.cpp:439 (OnCS_MOVE_REQ) +
// CSSender.cpp:599 (SendCS_MOVE_ACK). The legacy speed-hack check
// (fSpeed > 3.40) and ghost-mode branch are TODO — they need
// per-session player state that F8+ exposes.

#include "handlers.h"

#include "services/channel_presence.h"
#include "services/char_state_store.h"
#include <cmath>
#include "services/session_registry.h"
#include "services/session_validator.h"
#include "services/world_client.h"
#include "fourstory/db/co_offload.h"
#include "wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
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
    // The legacy code also does speed-hack detection (fSpeed > 3.40)
    // and ghost-mode handling — both are gameplay-policy work that
    // wraps the simple broadcast in this phase; folding them in
    // here would couple F7 to features the player-state service
    // (F8) hasn't materialized yet. TODO once F8 lands.
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
    // A MOVE cannot perform a world transfer; the selected map is authoritative.
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
