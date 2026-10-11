#include "handlers.h"
#include "config.h"
#include "services/char_state_store.h"
#include "services/main_transfer_runtime.h"
#include "services/player_service.h"
#include "services/session_registry.h"
#include "fourstory/db/co_offload.h"

namespace tmapsvr {
boost::asio::awaitable<void> OnSecureCodeReq(std::shared_ptr<tnetlib::AsioSession> client,
    std::uint16_t opcode,std::vector<std::byte> body,const HandlerContext& ctx) {
    // Original BOW excludes these three handlers, not disable/enable/check/time.
    if(ctx.mode==Mode::Bow&&(opcode==0x5580||opcode==0x5582||opcode==0x5588))co_return;
    const auto identity=ctx.session_reg?ctx.session_reg->Identity(client.get()):std::nullopt;
    if(!identity||identity->phase!=SessionPhase::Ready||identity->role!=MapSessionRole::Primary)co_return;
    DecodeSecureRequest(opcode,body);
    if(!ctx.char_state||!ctx.player_service||!ctx.skill_cooldown)throw std::runtime_error("Secure dependencies unavailable");
    const auto cid=identity->char_id;const auto original=ctx.char_state->Freeze(cid,[](auto&){});
    if(!original)co_return;
    SecureCommit committed;
    try {
        const auto before=transfer::PersistenceSnapshot(*original,identity->key,*ctx.skill_cooldown,SkillClockMs());
        auto* service=ctx.player_service;
        committed=co_await fourstory::db::CoOffloadIf(ctx.db_pool,[service,claim=identity->Claim(ctx.expected_group),opcode,&body,&before]{
            return service->UpdateSecureCode(claim,opcode,body,before);
        });
        if(!committed.snapshot)throw std::runtime_error("Missing secure commit state");
        ctx.char_state->Store(cid,*committed.snapshot);
    }catch(...) {
        auto uncertain=*original;uncertain.persistence_uncertain=true;ctx.char_state->Store(cid,uncertain);client->Close();throw;
    }
    if(committed.packet)co_await client->SendPacket(committed.packet->first,committed.packet->second);
}
}
