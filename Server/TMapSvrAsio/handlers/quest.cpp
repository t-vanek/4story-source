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
// Bounded, documented at each site: rewards deliver RT_GOLD + RT_EXP (gold
// straight to the purse via EarnMoney's raw-cooper semantics,
// TPlayer.cpp:2665; EXP accrues like a kill). RT_ITEM / RT_SKILL / RT_TITLE,
// the RM_SELECT / RM_PROB take-methods, the QCT_ accept-condition gate, and
// DB persistence of quest state are follow-ups (see services/quest_log.h).

#include "handlers.h"

#include "domain/character.h"
#include "domain/quest.h"
#include "domain/quest_def.h"
#include "services/char_state_store.h"
#include "services/client_senders.h"
#include "services/money.h"
#include "services/quest_chart.h"
#include "services/quest_engine.h"
#include "services/quest_log.h"
#include "services/quest_service.h"
#include "services/session_registry.h"
#include "services/player_service.h"
#include "services/monster_registry.h"
#include "services/main_transfer_runtime.h"
#include "fourstory/db/co_offload.h"
#include "wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace tmapsvr {

boost::asio::awaitable<void> OnNativeQuestReq(std::shared_ptr<tnetlib::AsioSession> client,
    std::uint16_t opcode,std::vector<std::byte> body,const HandlerContext& ctx) {
    const auto identity=ctx.session_reg?ctx.session_reg->Identity(client.get()):std::nullopt;
    if(!identity||identity->phase!=SessionPhase::Ready||identity->role!=MapSessionRole::Primary)co_return;
    const auto required=opcode==static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_NPCTALK_REQ)?2U:
        opcode==static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_QUESTEXEC_REQ)?9U:4U;
    if(body.size()!=required)throw std::runtime_error("Invalid quest request length");
    if(!ctx.char_state||!ctx.player_service||!ctx.quest_chart||!ctx.skill_cooldown||!ctx.monster_registry)
        throw std::runtime_error("Native quest dependencies unavailable");
    const auto cid=identity->char_id;
    const auto original=ctx.char_state->Freeze(cid,[](auto&){});if(!original)co_return;
    QuestCommit committed;
    try {
        const auto before=transfer::PersistenceSnapshot(*original,identity->key,*ctx.skill_cooldown,SkillClockMs());
        QuestMapFacts facts;facts.map=before.wMapID;facts.channel=identity->channel;
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
    for(const auto& [id,bytes]:committed.packets)co_await client->SendPacket(id,bytes);
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
