#pragma once

// Quest handlers — F7 Part 1.
//
// Wire references:
//   CS_QUESTEXEC_REQ  — CSHandler.cpp:3535
//   CS_QUESTDROP_REQ  — CSHandler.cpp:3590
//   CS_QUESTLIST_ACK  — CSSender.cpp:1861
//   CS_QUESTUPDATE_ACK — CSSender.cpp:1825
//   CS_QUESTCOMPLETE_ACK — CSSender.cpp:1843
//   CS_QUESTLIST_POSSIBLE_ACK — CSSender.cpp:1952

#include "asio_session.h"
#include "quest_engine.h"

#include <boost/asio/awaitable.hpp>
#include <memory>

namespace tmapsvr {

// F7: accept a quest. Source: CSHandler.cpp:3535.
boost::asio::awaitable<void> OnQuestExecReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    struct MapSessionState&              state,
    const tnetlib::DecodedPacket&        packet,
    const struct HandlerContext&         ctx);

// F7: abandon a quest. Source: CSHandler.cpp:3590.
boost::asio::awaitable<void> OnQuestDropReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    struct MapSessionState&              state,
    const tnetlib::DecodedPacket&        packet,
    const struct HandlerContext&         ctx);

// Send CS_QUESTLIST_ACK — all active quests + their term progress.
// Called on EnterMap / login to sync client state.
boost::asio::awaitable<void> SendQuestListAck(
    std::shared_ptr<tnetlib::AsioSession>         sess,
    const std::vector<const QuestState*>&         quests);

// Send CS_QUESTLIST_COMPLETE_ACK — list of completed quest ids.
boost::asio::awaitable<void> SendQuestListCompleteAck(
    std::shared_ptr<tnetlib::AsioSession>         sess,
    const std::vector<std::uint32_t>&             completed_ids);

// Send CS_QUESTUPDATE_ACK — single term progress update.
boost::asio::awaitable<void> SendQuestUpdateAck(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::uint32_t quest_id,
    std::uint32_t term_id,
    std::uint8_t  term_type,
    std::uint8_t  current_count,
    std::uint8_t  status);

// Send CS_QUESTCOMPLETE_ACK — quest completed or dropped.
boost::asio::awaitable<void> SendQuestCompleteAck(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::uint8_t  result,
    std::uint32_t quest_id,
    std::uint32_t term_id  = 0,
    std::uint8_t  term_type = 0,
    std::uint32_t drop_id  = 0);

// Send CS_QUESTLIST_POSSIBLE_ACK — quests available from an NPC.
boost::asio::awaitable<void> SendQuestListPossibleAck(
    std::shared_ptr<tnetlib::AsioSession>               sess,
    std::uint32_t                                       npc_id,
    std::uint8_t                                        npc_country,
    const std::vector<const QuestTemplate*>&            quests);

} // namespace tmapsvr
