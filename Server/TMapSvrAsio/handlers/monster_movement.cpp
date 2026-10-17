#include "handlers.h"
#include "services/char_state_store.h"
#include "services/native_cell_runtime.h"
#include "services/native_monster_motion.h"
#include "services/player_service.h"
#include "services/session_registry.h"

namespace tmapsvr {
boost::asio::awaitable<void> OnMonMoveReq(std::shared_ptr<tnetlib::AsioSession> session,
                                       std::vector<std::byte> body,const HandlerContext& ctx) {
    // Existing dispatch owns SessionOperation through this turn. Source accepts
    // both main and secondary host reporters; only their real local map is used.
    if(!ctx.session_reg||!ctx.char_state||!ctx.player_service||!ctx.player_service->HasNativeCellAuthority())co_return;
    const auto identity=ctx.session_reg->Identity(session.get());
    if(!session->IsOpen()||!identity||identity->phase!=SessionPhase::Ready)co_return;
    const auto records=DecodeNativeMonMoveRequest(body);
    if(!records){session->Close();co_return;}
    const auto snapshot=ctx.char_state->Get(identity->char_id);
    if(!snapshot||snapshot->dwCharID!=identity->char_id){session->Close();co_return;}
    try {
        // No SQL/World wait and no suspension during any cell/event/output
        // mutation. Service checks exact claim/map and retains actor isolation.
        const auto notices=ctx.player_service->MoveNativeMonsters(identity->Claim(ctx.expected_group),snapshot->wMapID,*records);
        PublishNativeCellNotices(ctx,notices);
    } catch(...) {
        // Publication can fail after the service has already captured/drained
        // its notices. Retire that graph too; never continue from partial output.
        ctx.player_service->StopNativeMonsterAi();session->Close();
    }
    co_return;
}
} // namespace tmapsvr
