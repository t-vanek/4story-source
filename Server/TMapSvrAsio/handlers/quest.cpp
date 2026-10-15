// Native immutable payloads dispatch through PostgreSQLMapService::ChangeQuest
// below. The older quest_log backend remains separate; its summary in the
// following historical comment does not describe native implementation parity.
// Quest handlers — CS_QUESTEXEC_REQ (accept / turn-in) and
// CS_QUESTDROP_REQ (abandon), wired to the data-driven quest engine.
//
// Faithful to the legacy flow (Quest.cpp / QuestComplete.cpp,
// CSHandler.cpp:3536 OnCS_QUESTEXEC_REQ / :3590 OnCS_QUESTDROP_REQ):
//   - A QT_COMPLETE quest is a *turn-in*: its QTT_COMPQUEST term names the
//     objective quest; if that quest's terms are all met the player gets
//     the reward (CS_QUESTCOMPLETE_ACK / QR_SUCCESS), else QR_TERM names
//     the unmet term.
//   - Any other type (QT_DIEMON / QT_MISSION / …) is a *register*: the
//     quest goes active with its objective terms at 0 (one
//     CS_QUESTUPDATE_ACK per term). The kill hook in combat.cpp advances
//     QTT_HUNT terms as monsters die.
//
// The legacy backend below delivers RT_GOLD + RT_EXP (gold
// straight to the purse via EarnMoney's raw-cooper semantics,
// TPlayer.cpp:2665; EXP accrues like a kill). RT_ITEM / RT_SKILL / RT_TITLE,
// the RM_SELECT / RM_PROB take-methods, the QCT_ accept-condition gate, and
// DB persistence remain limitations of that backend (services/quest_log.h).
// Native hydrated characters take OnNativeQuestReq and the transactional
// PostgreSQL implementation, including its explicit remaining branch guards.

#include "handlers.h"
#include "config.h"

#include "domain/character.h"
#include "domain/quest.h"
#include "domain/quest_def.h"
#include "services/char_state_store.h"
#include "services/client_senders.h"
#include "services/party_runtime.h"
#include "services/money.h"
#include "services/quest_chart.h"
#include "services/quest_engine.h"
#include "services/quest_log.h"
#include "services/quest_service.h"
#include "services/session_registry.h"
#include "services/player_service.h"
#include "services/monster_registry.h"
#include "services/channel_presence.h"
#include "services/world_client.h"
#include <cmath>
#include "services/main_transfer_runtime.h"
#include "services/secure_code.h"
#include "fourstory/db/co_offload.h"
#include "wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace tmapsvr {

boost::asio::awaitable<void> OnNativeQuestReq(std::shared_ptr<tnetlib::AsioSession> client,
    std::uint16_t opcode,std::vector<std::byte> body,const HandlerContext& ctx) {
    const auto identity=ctx.session_reg?ctx.session_reg->Identity(client.get()):std::nullopt;
    if(!identity||identity->phase!=SessionPhase::Ready||identity->role!=MapSessionRole::Primary)co_return;
    const auto message=static_cast<tnetlib::protocol::MessageId>(opcode);
    const bool revival=message==tnetlib::protocol::MessageId::CS_REVIVAL_REQ;
    const bool change_mode=message==tnetlib::protocol::MessageId::CS_CHGMODE_REQ;
    const bool drop_damage=message==tnetlib::protocol::MessageId::CS_DROPDAMAGE_REQ;
    const bool act_items=message==tnetlib::protocol::MessageId::CS_ACTITEMSUSE_REQ;
    const bool item_use=message==tnetlib::protocol::MessageId::CS_ITEMUSE_REQ;
    const bool recovery_tick=opcode==0&&ctx.mode==Mode::PvE;
    const bool source_projection=revival||change_mode||drop_damage||recovery_tick||act_items||item_use;
    if(revival||change_mode||drop_damage||act_items)if(ctx.mode!=Mode::PvE)co_return;
    if(source_projection) {
        const auto state=ctx.char_state?ctx.char_state->Get(identity->char_id):std::nullopt;
        const auto entry=ctx.presence?ctx.presence->FindEntry(identity->char_id):std::nullopt;
        if(!state||!state->payload||!entry||entry->map_id!=state->wMapID||entry->channel!=identity->channel||entry->session.lock().get()!=client.get())
            throw std::runtime_error("Native gameplay projection requires actual map presence and payload");
    }
    const bool soulmate=message==tnetlib::protocol::MessageId::CS_SOULMATESEARCH_REQ||
        message==tnetlib::protocol::MessageId::CS_SOULMATEREG_REQ||message==tnetlib::protocol::MessageId::CS_SOULMATEREGREADY_REQ||
        message==tnetlib::protocol::MessageId::MW_SOULMATESEARCH_REQ||message==tnetlib::protocol::MessageId::MW_SOULMATEREG_REQ;
    if(soulmate&&ctx.mode==Mode::Bow)co_return;
    if(soulmate&&(message==tnetlib::protocol::MessageId::CS_SOULMATESEARCH_REQ||message==tnetlib::protocol::MessageId::CS_SOULMATEREG_REQ||
        message==tnetlib::protocol::MessageId::CS_SOULMATEREGREADY_REQ)) {
        const auto state=ctx.char_state?ctx.char_state->Get(identity->char_id):std::nullopt;if(!state||!state->payload)co_return;
        // REG's source secure prompt precedes ProtectTutorial and decoding.
        if(message==tnetlib::protocol::MessageId::CS_SOULMATEREG_REQ&&state->payload->security.Blocks()) {
            const auto prompt=SecureUnlockPrompt(state->payload->security);
            co_await client->SendPacket(prompt.first,prompt.second);co_return;
        }
        if(state->bStartAct==2||(state->bStartAct==1&&state->wMapID==2010))co_return;
    }
    const bool mail=message==tnetlib::protocol::MessageId::CS_POSTSEND_REQ||message==tnetlib::protocol::MessageId::CS_POSTLIST_REQ||message==tnetlib::protocol::MessageId::CS_POSTVIEW_REQ||
        message==tnetlib::protocol::MessageId::CS_POSTGETITEM_REQ||message==tnetlib::protocol::MessageId::CS_POSTDEL_REQ||
        message==tnetlib::protocol::MessageId::CS_POSTRETURN_REQ;
    if(message==tnetlib::protocol::MessageId::CS_ITEMUSE_REQ||
       message==tnetlib::protocol::MessageId::CS_ITEMSELL_REQ||
       message==tnetlib::protocol::MessageId::CS_ITEMBUY_REQ||
       message==tnetlib::protocol::MessageId::CS_MAGICITEMBUY_REQ||
       message==tnetlib::protocol::MessageId::CS_POSTSEND_REQ||
       message==tnetlib::protocol::MessageId::CS_POSTGETITEM_REQ||
       message==tnetlib::protocol::MessageId::CS_POSTDEL_REQ||
       message==tnetlib::protocol::MessageId::CS_POSTRETURN_REQ||
       message==tnetlib::protocol::MessageId::CS_DURATIONREP_REQ||
       message==tnetlib::protocol::MessageId::CS_REFINE_REQ||
       message==tnetlib::protocol::MessageId::CS_ITEMUPGRADE_REQ) {
        if(!ctx.char_state)throw std::runtime_error("Native item state unavailable");
        const auto state=ctx.char_state->Get(identity->char_id);if(!state)co_return;
        // Original protected handlers prompt before reading the request body.
        // The dispatcher already holds this session's gameplay lease; no write
        // or timer freeze is needed for this early rejection. Repositories also
        // check security after locking and validating authoritative ownership.
        if(state->payload&&state->payload->security.Blocks()) {
            const auto prompt=SecureUnlockPrompt(state->payload->security);
            co_await client->SendPacket(prompt.first,prompt.second);co_return;
        }
        if(message==tnetlib::protocol::MessageId::CS_ITEMUPGRADE_REQ&&state->payload&&
           (state->payload->personal_store||state->payload->deal_status))co_return;
    }
    if(mail) {
        const auto state=ctx.char_state?ctx.char_state->Get(identity->char_id):std::nullopt;
        if(!state)co_return;
        if(state->bStartAct==2||(state->bStartAct==1&&state->wMapID==2010)||
            (message!=tnetlib::protocol::MessageId::CS_POSTLIST_REQ&&state->payload&&state->payload->tournament_player))co_return;
    }
    if(message==tnetlib::protocol::MessageId::CS_POSTSEND_REQ) {
        const auto state=ctx.char_state?ctx.char_state->Get(identity->char_id):std::nullopt;
        if(!state)co_return;
        if(state->payload&&(state->payload->deal_status||state->payload->personal_store)) {
            co_await client->SendPacket(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_POSTSEND_ACK),
                std::vector<std::byte>{std::byte{12}});co_return;
        }
    }
    const bool variable_request=message==tnetlib::protocol::MessageId::CS_POSTSEND_REQ||
        message==tnetlib::protocol::MessageId::CS_PROTECTEDADD_REQ||message==tnetlib::protocol::MessageId::CS_PROTECTEDERASE_REQ;
    const bool lottery=opcode==static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_SOULLOTTERY_REQ)||
        opcode==static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_SOULLOTTERY_TRIGGER_REQ);
    const auto required=opcode<=2||lottery?0U:
        change_mode?1U:
        drop_damage?12U:
        act_items?36U:
        message==tnetlib::protocol::MessageId::CS_POSTRETURN_REQ||message==tnetlib::protocol::MessageId::CS_PROTECTEDOPTION_REQ?5U:
        message==tnetlib::protocol::MessageId::CS_REVIVAL_REQ?13U:
        message==tnetlib::protocol::MessageId::CS_ITEMBUY_REQ?11U:
        message==tnetlib::protocol::MessageId::CS_MAGICITEMBUY_REQ?9U:
        message==tnetlib::protocol::MessageId::CS_NPCITEMLIST_REQ?2U:
        message==tnetlib::protocol::MessageId::CS_POSTLIST_REQ?2U:
        message==tnetlib::protocol::MessageId::CS_ITEMUPGRADE_REQ?10U:
        message==tnetlib::protocol::MessageId::CS_GAMBLEOPEN_REQ?6U:
        message==tnetlib::protocol::MessageId::CS_WASTEREFINE_REQ?2U:
        opcode==static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_DURATIONREP_REQ)?8U:
        opcode==static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_ITEMSELL_REQ)?5U:
        opcode==static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_NPCTALK_REQ)?2U:
        opcode==static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_QUESTEXEC_REQ)?9U:4U;
    if(soulmate) {
        if(body.size()>65527)throw std::runtime_error("Invalid soulmate request length");
    }else if(opcode==static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_ITEMUSE_REQ)) {
        if(body.size()<7||body.size()!=7U+5U*std::to_integer<unsigned>(body[6]))throw std::runtime_error("Invalid ITEMUSE request length");
    }else if(message==tnetlib::protocol::MessageId::CS_REFINE_REQ) {
        if(body.size()<8||body.size()!=8U+2U*std::to_integer<unsigned>(body[3]))throw std::runtime_error("Invalid REFINE request length");
    }else if(variable_request) {
        // Decode in the locked repository after the source early gates.
        if(body.size()<4||body.size()>65527)throw std::runtime_error("Invalid variable gameplay request length");
    }else if(body.size()!=required)throw std::runtime_error("Invalid quest request length");
    if(!ctx.char_state||!ctx.player_service||!ctx.quest_chart||!ctx.skill_cooldown||!ctx.monster_registry)
        throw std::runtime_error("Native quest dependencies unavailable");
    const auto cid=identity->char_id;
    const auto original=ctx.char_state->Freeze(cid,[](auto&){});if(!original)co_return;
    QuestCommit committed;
    try {
        const auto before=transfer::PersistenceSnapshot(*original,identity->key,*ctx.skill_cooldown,SkillClockMs());
        QuestMapFacts facts;facts.map=before.wMapID;facts.channel=identity->channel;
        facts.ordinary_recovery=recovery_tick;
        facts.ordinary_actitems=ctx.mode==Mode::PvE;
        facts.sample_ms=SkillClockMs();
        facts.world_registered=ctx.world_client&&ctx.world_client->IsRegistered();
        if(ctx.world_client)facts.world_events=ctx.world_client->EventSnapshot();
        if(message==tnetlib::protocol::MessageId::CS_SOULMATESEARCH_REQ) {
            if(!ctx.presence)throw std::runtime_error("Soulmate search neighbor state unavailable");
            const auto cell=[](float value)->int {
                if(!std::isfinite(value)||value<0||value>=65536)throw std::runtime_error("Soulmate search position is outside source range");
                return static_cast<std::uint16_t>(value)/64;
            };
            ctx.presence->ForEachInChannel(identity->channel,cid,[&](const ChannelPresenceEntry& entry,auto) {
                if(entry.map_id!=before.wMapID||std::abs(cell(entry.pos.x)-cell(before.fPosX))>1||
                   std::abs(cell(entry.pos.z)-cell(before.fPosZ))>1)return;
                const auto neighbor=ctx.char_state->Get(entry.char_id);
                if(neighbor&&std::abs(int(before.bLevel)-int(neighbor->bLevel))<3)
                    facts.soulmate_min_level=std::min(facts.soulmate_min_level,neighbor->bLevel);
            });
        }
        for(const auto& mon:ctx.monster_registry->ListInMap(facts.channel,facts.map)) {
            if(!mon.dwInstanceID)throw std::runtime_error("Quest actor snapshot contains reserved target zero");
            facts.monsters.emplace_back(mon.dwInstanceID,mon.wTemplateID);
        }
        auto* service=ctx.player_service;const auto* chart=ctx.quest_chart;
        committed=co_await fourstory::db::CoOffloadIf(ctx.db_pool,
            [service,chart,claim=identity->Claim(ctx.expected_group),opcode,&body,&before,&facts]{
                return service->ChangeQuest(claim,opcode,body,before,*chart,facts);
            });
        if(!committed.snapshot)throw std::runtime_error("Missing quest commit state");
        ctx.char_state->Store(cid,*committed.snapshot);
    }catch(...) {
        auto uncertain=*original;uncertain.persistence_uncertain=true;
        ctx.char_state->Store(cid,uncertain);client->Close();throw;
    }
    const auto& after=*committed.snapshot;
    if(source_projection) {
        const auto current=ctx.session_reg->Identity(client.get());
        if(!client->IsOpen()||!current||current->phase!=SessionPhase::Ready||current->char_id!=identity->char_id||
           current->user_id!=identity->user_id||current->key!=identity->key||current->channel!=identity->channel||
           current->connection_id!=identity->connection_id||current->authority_epoch!=identity->authority_epoch||current->role!=identity->role)co_return;
        if(revival)ctx.presence->UpdatePosition(cid,after.wMapID,{after.fPosX,after.fPosY,after.fPosZ});
    }
    std::vector<std::shared_ptr<tnetlib::AsioSession>> neighbors{client};
    const auto cell=[](float value)->int{return std::isfinite(value)&&value>=0&&value<65536?static_cast<std::uint16_t>(value)/64:-10000;};
    if(ctx.presence)ctx.presence->ForEachInChannel(identity->channel,cid,[&](const ChannelPresenceEntry& entry,auto session){
        if(entry.map_id==after.wMapID&&std::abs(cell(entry.pos.x)-cell(after.fPosX))<=1&&
           std::abs(cell(entry.pos.z)-cell(after.fPosZ))<=1)neighbors.push_back(std::move(session));
    });
    std::unordered_map<const tnetlib::AsioSession*,SessionIdentity> projection_proofs;
    std::unordered_set<const tnetlib::AsioSession*> unavailable;
    const auto same=[](const SessionIdentity& a,const SessionIdentity& b) {
        return a.char_id==b.char_id&&a.user_id==b.user_id&&a.key==b.key&&a.channel==b.channel&&a.connection_id==b.connection_id&&a.authority_epoch==b.authority_epoch&&a.role==b.role&&b.phase==SessionPhase::Ready;
    };
    const auto current_actor=[&]() -> std::optional<ChannelPresenceEntry> {
        const auto current=ctx.session_reg->Identity(client.get());
        const auto location=ctx.presence->FindEntry(cid);
        if(!client->IsOpen()||!current||!same(*identity,*current)||!location||
           location->session.lock().get()!=client.get()||location->channel!=identity->channel||location->map_id!=after.wMapID)return {};
        return location;
    };
    const auto deliver=[&](const std::shared_ptr<tnetlib::AsioSession>& recipient,const QuestPacket& packet)->boost::asio::awaitable<bool> {
        if(!source_projection){co_await recipient->SendPacket(packet.opcode,packet.body);co_return true;}
        if(!current_actor())co_return false;
        if(unavailable.contains(recipient.get()))co_return false;
        const auto id=ctx.session_reg->Identity(recipient.get());
        if(!recipient->IsOpen()||!id||id->phase!=SessionPhase::Ready)co_return false;
        // Nonblocking only: never wait for a second actor's checkpoint while
        // dispatch owns the actor. A busy/stale recipient may miss this event.
        std::optional<SessionOperation> operation;
        if(recipient.get()!=client.get()) {
            if(!ctx.session_reg->BeginGameplay(recipient.get()))co_return false;
            operation.emplace(*ctx.session_reg,recipient.get());
        }
        if(const auto it=projection_proofs.find(recipient.get());it!=projection_proofs.end()) {
            if(!same(it->second,*id))co_return false;
        }else {
            const auto live=ctx.char_state->Get(id->char_id);bool authorized=false;
            if(live)try {auto* service=ctx.player_service;
                authorized=bool(co_await fourstory::db::CoOffloadIf(ctx.db_pool,[service,claim=id->Claim(ctx.expected_group),&live]{return service->AuthorizeParty(claim,*live);}));
            }catch(...){authorized=false;}
            if(!authorized) {
                unavailable.insert(recipient.get());
                if(recipient.get()==client.get())client->Close();
                co_return false;
            }
            projection_proofs.emplace(recipient.get(),*id);
        }
        const auto actor_location=current_actor();
        const auto current=ctx.session_reg->Identity(recipient.get());
        if(!actor_location||!recipient->IsOpen()||!current||!same(*id,*current))co_return false;
        const auto location=ctx.presence->FindEntry(id->char_id);
        if(!location||location->session.lock().get()!=recipient.get()||location->channel!=identity->channel||id->channel!=identity->channel||location->map_id!=after.wMapID||
           std::abs(cell(location->pos.x)-cell(actor_location->pos.x))>1||std::abs(cell(location->pos.z)-cell(actor_location->pos.z))>1)co_return false;
        if(packet.audience==QuestAudience::Nearby&&std::hypot(location->pos.x-actor_location->pos.x,location->pos.z-actor_location->pos.z)>64.f)co_return false;
        co_await recipient->SendPacket(packet.opcode,packet.body);co_return true;
    };
    auto party_display=after;
    for(const auto& packet:committed.packets) {
        if(packet.opcode==static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_LEVEL_ACK)) {
            wire::Reader level(packet.body);std::uint32_t who=0;std::uint8_t value=0,show=0;
            if(!level.Read(who)||!level.Read(value)||!level.Read(show)||!level.Eof())throw std::runtime_error("Invalid committed level display");
            if(who==cid)party_display.bLevel=value;
        }
        if(packet.audience==QuestAudience::World) {
            if(!ctx.world_client||!ctx.world_client->IsRegistered()||
               !(co_await ctx.world_client->SendPacket(packet.opcode,packet.body))) {
                // The quest transaction is already durable. Do not replay it or
                // continue with a stale cluster projection after an unknown send.
                client->Close();co_return;
            }
        } else if(packet.audience==QuestAudience::Neighbors||packet.audience==QuestAudience::Nearby) {
            for(auto& session:neighbors) {
                const bool sent=co_await deliver(session,packet);
                if(sent&&session.get()==client.get()&&packet.opcode==static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_HPMP_ACK))
                    co_await PublishPartyVitals(client,party_display,packet.body,ctx);
            }
        } else {
            const bool sent=co_await deliver(client,packet);
            if(sent&&packet.opcode==static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_HPMP_ACK))
                co_await PublishPartyVitals(client,party_display,packet.body,ctx);
        }
    }
}

namespace {

// The objective (non-link) terms of a def, materialised as fresh progress
// rows at count 0 — the running-term set a freshly accepted quest starts
// with. (CQuest::FindRunningTerm lazily adds hunt terms as kills land;
// seeding them up front lets the client's quest tracker render at once.)
QuestProgressRow MakeAccepted(const QuestDef& def)
{
    QuestProgressRow p;
    p.dwQuestID = def.dwQuestID;
    for (const auto& t : def.terms)
    {
        if (t.bTermType == QTT_COMPQUEST) continue;   // turn-in link, not tracked
        p.terms.push_back(QuestTermRow{ t.dwTermID, t.bTermType, 0 });
    }
    return p;
}

// Grant a completed quest's rewards. RT_GOLD + RT_EXP land via char_state
// (EarnMoney's raw-cooper semantics + EXP accrual) and echo the resulting
// CS_MONEY / CS_EXP acks; other reward types are logged as deferred.
boost::asio::awaitable<void>
GrantRewards(std::shared_ptr<tnetlib::AsioSession> sess,
             std::uint32_t cid, const QuestDef& def,
             const HandlerContext& ctx)
{
    using tnetlib::protocol::MessageId;
    if (!ctx.char_state)
        co_return;

    for (const auto& rw : def.rewards)
    {
        switch (rw.bRewardType)
        {
        case RT_GOLD:
            ctx.char_state->Update(cid,
                [&](CharSnapshot& s) { AddMoneyToChar(s, rw.dwRewardID); });
            if (const auto s = ctx.char_state->Get(cid))
                co_await sess->SendPacket(
                    static_cast<std::uint16_t>(MessageId::CS_MONEY_ACK),
                    EncodeMoneyAck(s->dwGold, s->dwSilver, s->dwCooper));
            break;
        case RT_EXP:
            ctx.char_state->Update(cid,
                [&](CharSnapshot& s) { s.dwEXP += rw.dwRewardID; });
            if (const auto s = ctx.char_state->Get(cid))
                co_await sess->SendPacket(
                    static_cast<std::uint16_t>(MessageId::CS_EXP_ACK),
                    EncodeExpAck(s->dwEXP, 0, 0, 0));
            break;
        default:
            spdlog::info("quest reward type {} (id {}) deferred — only "
                "RT_GOLD/RT_EXP delivered this wave",
                static_cast<unsigned>(rw.bRewardType), rw.dwRewardID);
            break;
        }
    }
}

} // namespace

boost::asio::awaitable<void>
OnQuestExecReq(std::shared_ptr<tnetlib::AsioSession> sess,
               std::vector<std::byte>                body,
               const HandlerContext&                 ctx)
{
    using tnetlib::protocol::MessageId;

    const auto native=ctx.session_reg?ctx.session_reg->Identity(sess.get()):std::nullopt;
    const auto state=native&&ctx.char_state?ctx.char_state->Get(native->char_id):std::nullopt;
    if(state&&state->payload) {
        co_await OnNativeQuestReq(sess,static_cast<std::uint16_t>(MessageId::CS_QUESTEXEC_REQ),std::move(body),ctx);
        co_return;
    }

    // CS_QUESTEXEC_REQ body (legacy CSHandler.cpp:3536):
    //   DWORD dwQuestID, BYTE bRewardType, DWORD dwRewardID
    wire::Reader r(body.data(), body.size());
    std::uint32_t dwQuestID = 0, dwRewardID = 0;
    std::uint8_t  bRewardType = 0;
    if (!r.Read(dwQuestID) || !r.Read(bRewardType) || !r.Read(dwRewardID))
    {
        spdlog::warn("CS_QUESTEXEC_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }

    if (!ctx.session_reg || !ctx.quest_chart || !ctx.quest_log ||
        !ctx.quest_service)
        co_return;
    const auto found = ctx.session_reg->FindCharIdBySession(sess.get());
    if (!found)
        co_return;
    const std::uint32_t cid = *found;
    EnsureQuestsLoaded(*ctx.quest_log, *ctx.quest_service, cid);

    const QuestDef* def = ctx.quest_chart->Find(dwQuestID);
    if (!def)
    {
        spdlog::info("CS_QUESTEXEC_REQ char={} quest={} — unknown quest id",
            cid, dwQuestID);
        co_return;
    }

    // --- Turn-in: QT_COMPLETE evaluates the quest its QTT_COMPQUEST names
    //     (or itself when there is no link) and rewards on success. -------
    if (def->bType == QT_COMPLETE)
    {
        std::uint32_t target = dwQuestID;
        for (const auto& t : def->terms)
            if (t.bTermType == QTT_COMPQUEST) { target = t.dwTermID; break; }

        const QuestDef*   tdef  = ctx.quest_chart->Find(target);
        QuestProgressRow* tprog = ctx.quest_log->Find(cid, target);
        const QuestProgressRow  empty;                  // unaccepted → no progress
        const QuestProgressRow& prog = tprog ? *tprog : empty;

        if (tdef && quest_engine::IsComplete(prog, *tdef))
        {
            co_await GrantRewards(sess, cid, *tdef, ctx);
            co_await sess->SendPacket(
                static_cast<std::uint16_t>(MessageId::CS_QUESTCOMPLETE_ACK),
                EncodeQuestCompleteAck(QR_SUCCESS, target, 0, 0, 0));
            ctx.quest_log->Remove(cid, target);  // repeatable persist = follow-up
            spdlog::info("quest: char={} completed quest={} → rewarded",
                cid, target);
        }
        else
        {
            std::uint32_t term_id = 0;
            std::uint8_t  term_ty = 0;
            if (tdef)
                if (const auto* inc =
                        quest_engine::FirstIncompleteTerm(prog, *tdef))
                { term_id = inc->dwTermID; term_ty = inc->bTermType; }
            co_await sess->SendPacket(
                static_cast<std::uint16_t>(MessageId::CS_QUESTCOMPLETE_ACK),
                EncodeQuestCompleteAck(QR_TERM, target, term_id, term_ty, 0));
            spdlog::info("quest: char={} turn-in quest={} — terms unmet",
                cid, target);
        }
        co_return;
    }

    // --- Register: any other type goes active with its terms at 0. -------
    if (ctx.quest_log->Find(cid, dwQuestID))
    {
        spdlog::info("quest: char={} quest={} already active", cid, dwQuestID);
        co_return;
    }
    ctx.quest_log->Add(cid, MakeAccepted(*def));
    if (const auto* prog = ctx.quest_log->Find(cid, dwQuestID))
        for (const auto& t : prog->terms)
            co_await sess->SendPacket(
                static_cast<std::uint16_t>(MessageId::CS_QUESTUPDATE_ACK),
                EncodeQuestUpdateAck(dwQuestID, t.dwTermID, t.bTermType,
                    t.bCount, QTS_RUN));
    spdlog::info("quest: char={} accepted quest={} (type {})",
        cid, dwQuestID, static_cast<unsigned>(def->bType));
}

boost::asio::awaitable<void>
OnQuestDropReq(std::shared_ptr<tnetlib::AsioSession> sess,
               std::vector<std::byte>                body,
               const HandlerContext&                 ctx)
{
    using tnetlib::protocol::MessageId;
    const auto identity=ctx.session_reg?ctx.session_reg->Identity(sess.get()):std::nullopt;
    const auto snapshot=identity&&ctx.char_state?ctx.char_state->Get(identity->char_id):std::nullopt;
    if(snapshot&&snapshot->payload) {
        co_await OnNativeQuestReq(sess,static_cast<std::uint16_t>(MessageId::CS_QUESTDROP_REQ),std::move(body),ctx);
        co_return;
    }

    // CS_QUESTDROP_REQ body (legacy CSHandler.cpp:3590): DWORD dwQuestID
    wire::Reader r(body.data(), body.size());
    std::uint32_t dwQuestID = 0;
    if (!r.Read(dwQuestID))
    {
        spdlog::warn("CS_QUESTDROP_REQ: short body ({} bytes) — dropping",
            body.size());
        co_return;
    }

    if (!ctx.session_reg || !ctx.quest_log || !ctx.quest_service)
        co_return;
    const auto found = ctx.session_reg->FindCharIdBySession(sess.get());
    if (!found)
        co_return;
    const std::uint32_t cid = *found;
    EnsureQuestsLoaded(*ctx.quest_log, *ctx.quest_service, cid);

    const bool removed = ctx.quest_log->Remove(cid, dwQuestID);
    // Legacy acks the drop with QR_DROP regardless (the client closes the
    // tracker entry); dwDropID echoes the quest id.
    co_await sess->SendPacket(
        static_cast<std::uint16_t>(MessageId::CS_QUESTCOMPLETE_ACK),
        EncodeQuestCompleteAck(QR_DROP, dwQuestID, 0, 0, dwQuestID));
    spdlog::info("quest: char={} dropped quest={} (was active: {})",
        cid, dwQuestID, removed);
}

} // namespace tmapsvr
