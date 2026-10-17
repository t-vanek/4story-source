#include "native_cell_runtime.h"
#include "../handlers.h"
#include "char_state_store.h"
#include "session_registry.h"
#include "client_senders.h"
#include "MessageId.h"
#include "wire_codec.h"
#include <stdexcept>

namespace tmapsvr {
namespace {
std::shared_ptr<tnetlib::AsioSession> Current(const HandlerContext& ctx,const MapSessionClaim& claim,bool observer=false) {
    auto session=ctx.session_reg->Find(claim.char_id,claim.key);
    if(!session||!session->IsOpen())return {};
    const auto identity=ctx.session_reg->Identity(session.get());
    if(!identity||!(identity->phase==SessionPhase::Ready||(observer&&
        (identity->phase==SessionPhase::TransferOut||identity->phase==SessionPhase::TransferIn||identity->phase==SessionPhase::Loaded)))||
       !SameNativeCellPlayer(identity->Claim(ctx.expected_group),claim))return {};
    return session;
}
std::uint8_t PlayerColor(const CharSnapshot& actor,const CharSnapshot& observer) {
    // CTObjBase::CanFight/GetWarCountry + TMapType::GetAttackCountry,
    // ordinary non-BOW/non-BR OT_PC. The actor's comment uses this same color.
    const auto aid=[](const CharSnapshot& s){return s.payload?s.payload->aid_country:s.cluster.aid_country;};
    const auto war=[&](const CharSnapshot& s){const auto country=aid(s);return country!=3?country:s.bCountry;};
    return war(actor)==war(observer)||actor.bCountry==4||observer.bCountry==4?0:3;
}
}
void PublishNativeCellNotices(const HandlerContext& ctx,const NativeCellNotices& notices,std::span<const std::byte> movement) {
    using tnetlib::protocol::MessageId;
    if(!ctx.session_reg||!ctx.char_state)throw std::runtime_error("Native cell publication requires session/character state");
    for(const auto& notice:notices) {
        // An already entered observer keeps receiving cell deltas during an
        // ownership handoff. Initial Loaded players have no cell membership.
        auto recipient=Current(ctx,notice.recipient,true);
        if(!recipient)continue;
        std::vector<std::byte> packet;
        MessageId opcode;
        if(notice.kind==NativeCellNoticeKind::Leave) {
            // The actor may already be Closing/TransferOut. The captured
            // generation authorized removal; do not require a live actor here.
            opcode=MessageId::CS_LEAVE_ACK;
            wire::WritePOD(packet,notice.actor.char_id);
            wire::WritePOD<std::uint8_t>(packet,notice.flag?1:0);
        } else {
            auto actor=Current(ctx,notice.actor);
            if(!actor||notice.actor.role!=MapSessionRole::Primary)continue;
            if(notice.kind==NativeCellNoticeKind::Enter) {
                const auto snapshot=ctx.char_state->Get(notice.actor.char_id),observer=ctx.char_state->Get(notice.recipient.char_id);
                if(!snapshot||!snapshot->payload||!observer) {recipient->Close();continue;}
                opcode=MessageId::CS_ENTER_ACK;
                packet=EncodeEnterAck(*snapshot,{snapshot->fPosX,snapshot->fPosY,snapshot->fPosZ},
                    PlayerColor(*snapshot,*observer),notice.flag?1:0);
            } else {
                if(movement.size()!=27)throw std::runtime_error("Native cell move publication requires complete MOVE_ACK");
                opcode=MessageId::CS_MOVE_ACK;packet.assign(movement.begin(),movement.end());
            }
        }
        recipient->PostPacket(static_cast<std::uint16_t>(opcode),std::move(packet));
    }
}
}
