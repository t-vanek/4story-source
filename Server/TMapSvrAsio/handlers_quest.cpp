// Quest handlers — F7.
//
// Wire helpers for CS_QUEST* ACK packets + request handlers.
//
// Legacy references:
//   CSHandler.cpp:3535 — OnCS_QUESTEXEC_REQ
//   CSHandler.cpp:3590 — OnCS_QUESTDROP_REQ
//   CSSender.cpp:1825  — SendCS_QUESTUPDATE_ACK
//   CSSender.cpp:1843  — SendCS_QUESTCOMPLETE_ACK
//   CSSender.cpp:1861  — SendCS_QUESTLIST_ACK
//   CSSender.cpp:1952  — SendCS_QUESTLIST_POSSIBLE_ACK

#include "handlers_quest.h"
#include "handlers.h"
#include "handlers_combat.h"  // SendExpAck
#include "handlers_items.h"   // SendAddItemAck, ItemInstance, InvenType
#include "wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

namespace tmapsvr {

using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

// ---------------------------------------------------------------------------
// CS_QUESTUPDATE_ACK
// ---------------------------------------------------------------------------

boost::asio::awaitable<void>
SendQuestUpdateAck(std::shared_ptr<tnetlib::AsioSession> sess,
                   std::uint32_t quest_id,
                   std::uint32_t term_id,
                   std::uint8_t  term_type,
                   std::uint8_t  current_count,
                   std::uint8_t  status)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint32_t>(body, quest_id);
    wire::WritePOD<std::uint32_t>(body, term_id);
    wire::WritePOD<std::uint8_t> (body, term_type);
    wire::WritePOD<std::uint8_t> (body, current_count);
    wire::WritePOD<std::uint8_t> (body, status);
    co_await sess->SendPacket(
        ToUint16(MessageId::CS_QUESTUPDATE_ACK),
        std::span<const std::byte>(body.data(), body.size()));
}

// ---------------------------------------------------------------------------
// CS_QUESTCOMPLETE_ACK
// ---------------------------------------------------------------------------

boost::asio::awaitable<void>
SendQuestCompleteAck(std::shared_ptr<tnetlib::AsioSession> sess,
                     std::uint8_t  result,
                     std::uint32_t quest_id,
                     std::uint32_t term_id,
                     std::uint8_t  term_type,
                     std::uint32_t drop_id)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint8_t> (body, result);
    wire::WritePOD<std::uint32_t>(body, quest_id);
    wire::WritePOD<std::uint32_t>(body, term_id);
    wire::WritePOD<std::uint8_t> (body, term_type);
    wire::WritePOD<std::uint32_t>(body, drop_id);
    co_await sess->SendPacket(
        ToUint16(MessageId::CS_QUESTCOMPLETE_ACK),
        std::span<const std::byte>(body.data(), body.size()));
}

// ---------------------------------------------------------------------------
// CS_QUESTLIST_ACK
// ---------------------------------------------------------------------------

boost::asio::awaitable<void>
SendQuestListAck(std::shared_ptr<tnetlib::AsioSession>         sess,
                 const std::vector<const QuestState*>&         quests)
{
    std::vector<std::byte> body;
    const auto count = static_cast<std::uint8_t>(quests.size());
    wire::WritePOD<std::uint8_t>(body, count);

    for (const auto* q : quests)
    {
        if (!q) continue;
        wire::WritePOD<std::uint32_t>(body, q->quest_id);
        wire::WritePOD<std::uint8_t> (body, q->quest_type);
        wire::WritePOD<std::uint8_t> (body, q->count_max);
        const auto tc = static_cast<std::uint8_t>(q->terms.size());
        wire::WritePOD<std::uint8_t>(body, tc);
        for (const auto& t : q->terms)
        {
            wire::WritePOD<std::uint32_t>(body, t.term_id);
            wire::WritePOD<std::uint8_t> (body, t.term_type);
            wire::WritePOD<std::uint8_t> (body, t.target_count);
            wire::WritePOD<std::uint8_t> (body, t.current_count);
            wire::WritePOD<std::uint8_t> (body, t.status);
        }
    }

    co_await sess->SendPacket(
        ToUint16(MessageId::CS_QUESTLIST_ACK),
        std::span<const std::byte>(body.data(), body.size()));
}

// ---------------------------------------------------------------------------
// CS_QUESTLIST_COMPLETE_ACK
// ---------------------------------------------------------------------------

boost::asio::awaitable<void>
SendQuestListCompleteAck(std::shared_ptr<tnetlib::AsioSession>         sess,
                         const std::vector<std::uint32_t>&             ids)
{
    std::vector<std::byte> body;
    const auto count = static_cast<std::uint8_t>(ids.size());
    wire::WritePOD<std::uint8_t>(body, count);
    for (auto id : ids)
        wire::WritePOD<std::uint32_t>(body, id);
    co_await sess->SendPacket(
        ToUint16(MessageId::CS_QUESTLIST_COMPLETE_ACK),
        std::span<const std::byte>(body.data(), body.size()));
}

// ---------------------------------------------------------------------------
// CS_QUESTLIST_POSSIBLE_ACK
// ---------------------------------------------------------------------------

boost::asio::awaitable<void>
SendQuestListPossibleAck(std::shared_ptr<tnetlib::AsioSession>               sess,
                         std::uint32_t                                       npc_id,
                         std::uint8_t                                        npc_country,
                         const std::vector<const QuestTemplate*>&            quests)
{
    std::vector<std::byte> body;
    // One NPC block
    wire::WritePOD<std::uint8_t>(body, 1u);  // npc_count=1
    wire::WritePOD<std::uint16_t>(body, static_cast<std::uint16_t>(npc_id));
    wire::WritePOD<std::uint8_t> (body, npc_country);
    const auto qc = static_cast<std::uint8_t>(quests.size());
    wire::WritePOD<std::uint8_t>(body, qc);
    for (const auto* q : quests)
    {
        if (!q) continue;
        wire::WritePOD<std::uint32_t>(body, q->quest_id);
        wire::WritePOD<std::uint8_t> (body, q->quest_type);
    }
    co_await sess->SendPacket(
        ToUint16(MessageId::CS_QUESTLIST_POSSIBLE_ACK),
        std::span<const std::byte>(body.data(), body.size()));
}

// ---------------------------------------------------------------------------
// DispatchQuestEvents — central reward/ACK handler for all quest term types
// ---------------------------------------------------------------------------
//
// Called from OnActionReq (Hunt), OnMonItemTakeReq (GetItem), OnNpcTalkReq (Talk).
// For each event: sends CS_QUESTUPDATE_ACK; on quest completion grants rewards
// (CS_EXP_ACK, CS_ADDITEM_ACK) then CS_QUESTCOMPLETE_ACK.

boost::asio::awaitable<void>
DispatchQuestEvents(std::shared_ptr<tnetlib::AsioSession>       sess,
                    MapSessionState&                             state,
                    const HandlerContext&                        ctx,
                    const std::vector<QuestProgressEvent>&       events)
{
    for (const auto& ev : events)
    {
        co_await SendQuestUpdateAck(sess,
            ev.quest_id, ev.term_id, ev.term_type,
            ev.new_count,
            ev.term_complete ? QuestTermStatus::Success
                             : QuestTermStatus::Running);

        if (!ev.quest_complete) continue;

        spdlog::info("Quest {} completed by uid={}", ev.quest_id, state.user_id);

        // Rewards — requires chart + snapshot
        if (ctx.quest_engine && state.snapshot)
        {
            const auto* chart = ctx.quest_engine->Chart();
            const auto* tmpl  = chart ? chart->GetQuest(ev.quest_id) : nullptr;
            if (tmpl)
            {
                // Exp reward
                if (tmpl->reward.exp_reward > 0)
                {
                    state.snapshot->exp += tmpl->reward.exp_reward;
                    co_await SendExpAck(sess, state.snapshot->exp, 0u, 0u);
                }
                // Item reward
                if (tmpl->reward.item_id != 0 && ctx.inventory_svc)
                {
                    ItemInstance rw{};
                    rw.item_id    = tmpl->reward.item_id;
                    rw.count      = tmpl->reward.item_count ? tmpl->reward.item_count : 1u;
                    rw.inven_type = InvenType::Main;
                    rw.inven_id   = 0;
                    ctx.inventory_svc->AddItem(state.char_id, rw);
                    co_await SendAddItemAck(sess,
                        static_cast<std::uint8_t>(InvenType::Main), rw);
                }
            }
        }

        co_await SendQuestCompleteAck(sess, QuestResult::Success,
            ev.quest_id, ev.term_id, ev.term_type, 0u);
    }
}

// ---------------------------------------------------------------------------
// CS_QUESTPOSEXEC_REQ handler
// ---------------------------------------------------------------------------
//
// Confirms that a position-based (Hunt/area) quest term was fulfilled at the
// player's current location. Client sends when it arrives at the term point.
//
// Wire body: DWORD dwQuestID, DWORD dwTermID
// Source: CSHandler.cpp:20997

boost::asio::awaitable<void>
OnQuestPosExecReq(std::shared_ptr<tnetlib::AsioSession> sess,
                  MapSessionState&                     state,
                  const tnetlib::DecodedPacket&        packet,
                  const HandlerContext&                ctx)
{
    if (!state.connected || !state.snapshot) co_return;

    wire::Reader r(packet.body.data(), packet.body.size());
    std::uint32_t quest_id = 0, term_id = 0;
    if (!r.Read(quest_id) || !r.Read(term_id))
    {
        spdlog::warn("CS_QUESTPOSEXEC_REQ malformed uid={}", state.user_id);
        co_return;
    }

    if (!ctx.quest_engine) co_return;

    // Directly complete the specified Hunt term by quest+term id.
    // Legacy: CSHandler.cpp:20997 — FindTerm(dwTermID, QTT_HUNT) + FinishTerm.
    auto ev = ctx.quest_engine->ForceCompleteHuntTerm(
        state.char_id, quest_id, term_id);

    if (ev.empty())
    {
        spdlog::debug("CS_QUESTPOSEXEC_REQ: no matching term uid={} quest={} term={}",
            state.user_id, quest_id, term_id);
        co_return;
    }

    co_await DispatchQuestEvents(sess, state, ctx, ev);
}

// ---------------------------------------------------------------------------
// CS_QUESTEXEC_REQ handler
// ---------------------------------------------------------------------------
//
// Wire body: DWORD dwQuestID, BYTE bRewardType, DWORD dwRewardID
// Source: CSHandler.cpp:3535

boost::asio::awaitable<void>
OnQuestExecReq(std::shared_ptr<tnetlib::AsioSession> sess,
               MapSessionState&                     state,
               const tnetlib::DecodedPacket&        packet,
               const HandlerContext&                ctx)
{
    if (!state.connected || !state.snapshot) co_return;

    wire::Reader r(packet.body.data(), packet.body.size());
    std::uint32_t quest_id    = 0;
    std::uint8_t  reward_type = 0;
    std::uint32_t reward_id   = 0;
    if (!r.Read(quest_id) || !r.Read(reward_type) || !r.Read(reward_id))
    {
        spdlog::warn("CS_QUESTEXEC_REQ malformed uid={}", state.user_id);
        co_return;
    }

    if (!ctx.quest_engine)
    {
        co_await SendQuestCompleteAck(sess, QuestResult::Failed, quest_id);
        co_return;
    }

    const bool started = ctx.quest_engine->StartQuest(
        state.char_id, quest_id);

    if (!started)
    {
        spdlog::debug("CS_QUESTEXEC_REQ: quest_id={} already active or unknown",
            quest_id);
        co_return;
    }

    spdlog::info("CS_QUESTEXEC_REQ uid={} started quest_id={}",
        state.user_id, quest_id);

    // Send updated active quest list
    const auto active = ctx.quest_engine->GetActiveQuests(state.char_id);
    co_await SendQuestListAck(sess, active);
}

// ---------------------------------------------------------------------------
// CS_QUESTDROP_REQ handler
// ---------------------------------------------------------------------------
//
// Wire body: DWORD dwQuestID
// Source: CSHandler.cpp:3590

boost::asio::awaitable<void>
OnQuestDropReq(std::shared_ptr<tnetlib::AsioSession> sess,
               MapSessionState&                     state,
               const tnetlib::DecodedPacket&        packet,
               const HandlerContext&                ctx)
{
    if (!state.connected || !state.snapshot) co_return;

    wire::Reader r(packet.body.data(), packet.body.size());
    std::uint32_t quest_id = 0;
    if (!r.Read(quest_id))
    {
        spdlog::warn("CS_QUESTDROP_REQ malformed uid={}", state.user_id);
        co_return;
    }

    if (!ctx.quest_engine) co_return;

    const bool dropped = ctx.quest_engine->DropQuest(
        state.char_id, quest_id);

    spdlog::info("CS_QUESTDROP_REQ uid={} quest_id={} {}",
        state.user_id, quest_id, dropped ? "dropped" : "not found");

    co_await SendQuestCompleteAck(sess, QuestResult::Drop, quest_id);

    // Sync updated list
    const auto active = ctx.quest_engine->GetActiveQuests(state.char_id);
    co_await SendQuestListAck(sess, active);
}

} // namespace tmapsvr
