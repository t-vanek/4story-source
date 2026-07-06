#include "handlers.h"
#include "../senders/senders.h"
#include "../services/chat_constants.h"
#include "../services/event_constants.h"
#include "../services/event_registry.h"
#include "../services/event_info_codec.h"
#include "../services/lucky_event_codec.h"
#include "../wire_codec.h"

#include "fourstory/db/co_offload.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <ctime>
#include <random>
#include <cstdlib>
#include <string>

namespace tworldsvr::handlers {

namespace {

// TCONTRY_N (NetCode.h:1095) — the neutral country the operator
// announcement is tagged with.
constexpr std::uint8_t kCountryNeutral = 3;

} // namespace

boost::asio::awaitable<void>
OnEventQuarterReq(std::shared_ptr<PeerSession> peer,
                  std::vector<std::byte>       body,
                  const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.peers)
    {
        spdlog::warn("OnEventQuarterReq[{}]: peers not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint8_t day = 0, hour = 0, minute = 0;
    std::string  present;
    if (!r.Read(day) || !r.Read(hour) || !r.Read(minute) ||
        !r.ReadString(present))
    {
        spdlog::warn("OnEventQuarterReq[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    // Pick the present bucket once so every map shows the same one
    // (legacy rand() % 100).
    const std::uint8_t select = static_cast<std::uint8_t>(std::rand() % 100);

    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwEventQuarterReq(p, day, hour, minute, select,
            present);
    co_return;
}

boost::asio::awaitable<void>
OnEventQuarterNotifyReq(std::shared_ptr<PeerSession> peer,
                        std::vector<std::byte>       body,
                        const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.peers)
    {
        spdlog::warn("OnEventQuarterNotifyReq[{}]: peers not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::string announce;
    if (!r.ReadString(announce))
    {
        spdlog::warn("OnEventQuarterNotifyReq[{}]: short body", ip);
        co_return;
    }

    // Broadcast the announcement as a world-chat line from the
    // operator. The operator display name (legacy GetSvrMsg(
    // NAME_OPERATOR)) is deferred — same server-message-table gap as
    // the W4-5 operator-whisper — so the sender name is left empty.
    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwChatReq(p, /*char_id=*/0, /*key=*/0,
            /*channel=*/0, /*sender_id=*/0, /*sender_name=*/std::string{},
            kCountryNeutral, kCountryNeutral, /*type=*/chat::kWorld,
            /*group=*/chat::kWorld, /*target_id=*/0, announce);
    co_return;
}

boost::asio::awaitable<void>
OnCtEventMsgReq(std::shared_ptr<PeerSession> peer,
                std::vector<std::byte>       body,
                const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.peers)
    {
        spdlog::warn("OnCtEventMsgReq[{}]: peers not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint8_t event_id = 0;
    std::uint8_t msg_type = 0;
    std::string  msg;
    if (!r.Read(event_id) || !r.Read(msg_type) || !r.ReadString(msg))
    {
        spdlog::warn("OnCtEventMsgReq[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwEventMsgReq(p, event_id, msg_type, msg);
    co_return;
}

namespace {

std::mt19937& LotteryRng()
{
    static std::mt19937 rng{std::random_device{}()};
    return rng;
}

// Split the legacy "title|message" LotMsg (CString::Tokenize on the
// first pipe; no pipe -> whole string is the title, empty message).
void SplitLotMsg(const std::string& lot_msg, std::string& title,
                 std::string& message)
{
    const auto pos = lot_msg.find('|');
    if (pos == std::string::npos)
    {
        title = lot_msg;
        message.clear();
        return;
    }
    title   = lot_msg.substr(0, pos);
    message = lot_msg.substr(pos + 1);
}

struct LotteryWinnerPool
{
    std::vector<std::pair<std::uint32_t, std::string>> entries;
};

// Legacy LotteryItem (TWorldSvr.cpp:7115): draw winners without
// repetition across ALL rows, mail each one the prize, then fan the
// winner board to every map. Pool exhaustion flushes the partial
// board and returns early (legacy parity).
boost::asio::awaitable<void>
RunLotteryEvent(const HandlerContext& ctx, const EventInfoTail& tail)
{
    if (!ctx.chars || !ctx.peers)
        co_return;
    auto peers = ctx.peers->Snapshot();
    if (peers.empty())
        co_return;                       // legacy m_mapSERVER.empty()

    std::string title, message;
    SplitLotMsg(tail.lot_msg, title, message);

    // Candidate pool: map-type filter (byte-truncated map id) or the
    // whole online population.
    const auto map_type = static_cast<std::uint8_t>(tail.map_id);
    LotteryWinnerPool pool;
    for (std::uint32_t id : ctx.chars->SnapshotIds())
    {
        auto c = ctx.chars->Find(id);
        if (!c) continue;
        std::lock_guard g(c->lock);
        if (map_type != 0 &&
            !event_type::CheckEventMapId(map_type, c->map_id))
            continue;
        pool.entries.emplace_back(c->char_id, c->name);
    }

    struct Board { EventLotteryRow row; std::vector<std::string> names; };
    std::vector<Board> boards;

    auto broadcast = [&]() -> boost::asio::awaitable<void>
    {
        if (boards.empty())
            co_return;
        std::vector<senders::LotteryBoardRow> rows;
        for (const auto& b : boards)
            rows.push_back({b.row.item_id, b.row.num, b.names});
        for (auto& p : ctx.peers->Snapshot())
            co_await senders::SendMwEventMsgLotteryReq(p, tail.title,
                rows);
        co_return;
    };

    for (const auto& row : tail.lottery)
    {
        Board board{};
        board.row = row;
        for (std::uint16_t j = 0; j < row.winner; ++j)
        {
            if (pool.entries.empty())
            {
                if (!board.names.empty())
                    boards.push_back(std::move(board));
                co_await broadcast();
                co_return;               // legacy early return
            }
            std::uniform_int_distribution<std::size_t> dist(
                0, pool.entries.size() - 1);
            const std::size_t pick = dist(LotteryRng());
            const auto [win_id, win_name] = pool.entries[pick];

            // Legacy SendPost(WPT_LOTITEM, ...) mails through the
            // FIRST registered map (the post table is DB-backed).
            co_await senders::SendMwWorldPostLotItemReq(peers.front(),
                event_type::kWptLotItem, win_id, win_name, title,
                message, row.item_id, row.num, /*use_time=*/0);

            board.names.push_back(win_name);
            pool.entries.erase(pool.entries.begin() +
                               static_cast<std::ptrdiff_t>(pick));
        }
        if (!board.names.empty())
            boards.push_back(std::move(board));
        if (pool.entries.empty())
        {
            co_await broadcast();
            co_return;
        }
    }
    co_await broadcast();
    co_return;
}

// Legacy GiftTime (TWorldSvr.cpp:7273): every online char inside the
// [HIBYTE(value), LOBYTE(value)] level band gets the first lottery
// row mailed (use_time = the row's winner field). No board fan-out.
boost::asio::awaitable<void>
RunGiftTimeEvent(const HandlerContext& ctx, const EventInfoTail& tail)
{
    if (!ctx.chars || !ctx.peers)
        co_return;
    auto peers = ctx.peers->Snapshot();
    if (peers.empty())
        co_return;
    if (tail.lottery.empty())
    {
        // Legacy dereferences m_vLOTTERY.at(0) unguarded — an empty
        // row list would throw there; we drop with a log instead.
        spdlog::warn("RunGiftTimeEvent: no lottery row — dropped");
        co_return;
    }
    std::string title, message;
    SplitLotMsg(tail.lot_msg, title, message);
    const auto& row = tail.lottery.front();
    const std::uint8_t min_level =
        static_cast<std::uint8_t>(tail.value >> 8);
    const std::uint8_t max_level =
        static_cast<std::uint8_t>(tail.value & 0xFF);

    for (std::uint32_t id : ctx.chars->SnapshotIds())
    {
        auto c = ctx.chars->Find(id);
        if (!c) continue;
        std::uint32_t char_id = 0;
        std::string   name;
        {
            std::lock_guard g(c->lock);
            if (c->level < min_level || c->level > max_level)
                continue;
            char_id = c->char_id;
            name    = c->name;
        }
        co_await senders::SendMwWorldPostLotItemReq(peers.front(),
            event_type::kWptLotItem, char_id, name, title, message,
            row.item_id, row.num, row.winner);
    }
    co_return;
}

} // namespace

boost::asio::awaitable<void>
OnCtEventUpdateReq(std::shared_ptr<PeerSession> peer,
                   std::vector<std::byte>       body,
                   const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.peers || !ctx.events)
    {
        spdlog::warn("OnCtEventUpdateReq[{}]: peers/events not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint8_t  event_id = 0;
    std::uint16_t value    = 0;
    std::uint32_t dw_index = 0;
    std::uint8_t  b_id     = 0;
    if (!r.Read(event_id) || !r.Read(value) || !r.Read(dw_index)
        || !r.Read(b_id))
    {
        spdlog::warn("OnCtEventUpdateReq[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    // Legacy SSHandler.cpp:276 — drop event_ids past the table.
    if (event_id > event_type::kCount)
    {
        spdlog::debug("OnCtEventUpdateReq[{}]: event_id={} > kCount={} "
                      "— dropped", ip, event_id, event_type::kCount);
        co_return;
    }

    // Legacy LOTTERY / GIFTTIME short-circuit (SSHandler.cpp:279-292):
    // both run the reward subsystem and return WITHOUT storing or
    // broadcasting the event. W6-47 ports the full EVENTINFO tail
    // parse + the reward runs (LotteryItem / GiftTime,
    // TWorldSvr.cpp:7115 / 7273).
    if (b_id == event_type::kLottery || b_id == event_type::kGiftTime)
    {
        EventInfoTail tail{};
        if (!ReadEventInfoTail(r, tail))
        {
            spdlog::warn("OnCtEventUpdateReq[{}]: malformed "
                         "LOTTERY/GIFTTIME EVENTINFO ({} bytes)", ip,
                body.size());
            co_return;
        }
        if (!tail.state)
            co_return;                    // legacy `if(m_bState)` gate
        if (b_id == event_type::kLottery)
            co_await RunLotteryEvent(ctx, tail);
        else
            co_await RunGiftTimeEvent(ctx, tail);
        co_return;
    }

    // Capture the opaque EVENTINFO body — everything past the outer
    // (event_id, value) pair. The dw_index + b_id we just peeked at
    // belong inside that body too, so we snapshot from offset 3 (the
    // outer header is BYTE + WORD = 3 bytes).
    constexpr std::size_t kOuterHeader =
        sizeof(std::uint8_t) + sizeof(std::uint16_t);
    std::vector<std::byte> event_body(
        body.begin() + kOuterHeader, body.end());

    // Mirror legacy SSHandler.cpp:294-298 — erase any existing entry
    // for this index, insert the new entry only if value != 0
    // (legacy "wValue==0 deactivates"). Always re-broadcast so peers
    // see the deactivation too (legacy broadcasts unconditionally).
    ctx.events->Erase(dw_index);
    if (value != 0)
    {
        TEventInfo info{};
        info.event_id = event_id;
        info.value    = value;
        info.dw_index = dw_index;
        info.b_id     = b_id;
        info.body     = event_body;
        ctx.events->Set(std::move(info));
    }

    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwEventUpdateReq(p, event_id, value,
            event_body);
    co_return;
}

boost::asio::awaitable<void>
OnCtEventQuarterListReq(std::shared_ptr<PeerSession> peer,
                        std::vector<std::byte>       body,
                        const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.ctrl_svr)
    {
        spdlog::warn("OnCtEventQuarterListReq[{}]: ctrl_svr not wired",
            ip);
        co_return;
    }
    if (!ctx.lucky_repo)
    {
        spdlog::warn("OnCtEventQuarterListReq[{}]: lucky_repo not "
                     "wired - dropping (no DB configured)", ip);
        co_return;
    }
    wire::Reader r(body.data(), body.size());
    std::uint32_t manager = 0;
    std::uint8_t  day = 0;
    if (!r.Read(manager) || !r.Read(day))
    {
        spdlog::warn("OnCtEventQuarterListReq[{}]: short body "
                     "({} bytes)", ip, body.size());
        co_return;
    }
    const auto rows = co_await fourstory::db::CoOffloadIf(ctx.db_pool,
        [repo = ctx.lucky_repo, day] { return repo->List(day); });
    if (auto cs = ctx.ctrl_svr->Get())
        co_await senders::SendCtEventQuarterListAck(cs, manager, rows);
    else
        spdlog::info("OnCtEventQuarterListReq[{}]: ctrl-svr offline - "
                     "{} row(s) dropped", ip, rows.size());
    co_return;
}

boost::asio::awaitable<void>
OnCtEventQuarterUpdateReq(std::shared_ptr<PeerSession> peer,
                          std::vector<std::byte>       body,
                          const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.ctrl_svr)
    {
        spdlog::warn("OnCtEventQuarterUpdateReq[{}]: ctrl_svr not "
                     "wired", ip);
        co_return;
    }
    if (!ctx.lucky_repo)
    {
        spdlog::warn("OnCtEventQuarterUpdateReq[{}]: lucky_repo not "
                     "wired - dropping (no DB configured)", ip);
        co_return;
    }
    wire::Reader r(body.data(), body.size());
    std::uint32_t manager = 0;
    std::uint8_t  type = 0;
    LuckyEvent    event{};
    if (!r.Read(manager) || !r.Read(type) || !ReadLuckyEvent(r, event))
    {
        spdlog::warn("OnCtEventQuarterUpdateReq[{}]: malformed body "
                     "({} bytes)", ip, body.size());
        co_return;
    }

    const auto res = co_await fourstory::db::CoOffloadIf(ctx.db_pool,
        [repo = ctx.lucky_repo, type, event]
        { return repo->Update(type, event); });

    // Legacy keeps bRet=TRUE and echoes the unresolved event when
    // the SP never ran; the ADD id adoption + name fill come from
    // the SP outputs. The in-memory EVQT scheduler mirror is the
    // lucky-event runtime slice.
    std::uint8_t ret = 1;
    LuckyEvent   echo = event;
    if (res)
    {
        ret  = res->ret;
        echo = res->event;

        // W6-48: mirror the successful edit into the running
        // scheduler (legacy OnDM_EVENTQUARTERUPDATE_ACK m_mapEVQT
        // maintenance).
        if (ctx.evqt)
        {
            const auto now =
                static_cast<std::int64_t>(std::time(nullptr));
            EventQuarterEntry entry{};
            entry.id = echo.id;
            entry.day = echo.day;
            entry.hour = echo.hour;
            entry.minute = echo.minute;
            entry.present = echo.present;
            entry.announce = echo.announce;
            if (type == kEkDel)
                ctx.evqt->ApplyDel(echo.id);
            else if (type == kEkAdd)
                ctx.evqt->ApplyAdd(entry, now);
            else if (type == kEkUpdate)
                ctx.evqt->ApplyUpdate(entry, now);
        }
    }
    if (auto cs = ctx.ctrl_svr->Get())
        co_await senders::SendCtEventQuarterUpdateAck(cs, ret, manager,
            type, echo);
    else
        spdlog::info("OnCtEventQuarterUpdateReq[{}]: ctrl-svr offline "
                     "- update ack dropped", ip);
    co_return;
}

namespace {

// Shared with OnEventQuarterReq: broadcast the present handout.
boost::asio::awaitable<void>
BroadcastEventQuarterPresent(const HandlerContext& ctx,
                             std::uint8_t day, std::uint8_t hour,
                             std::uint8_t minute,
                             const std::string& present)
{
    if (!ctx.peers) co_return;
    const std::uint8_t select =
        static_cast<std::uint8_t>(std::rand() % 100);
    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwEventQuarterReq(p, day, hour, minute,
            select, present);
    co_return;
}

// Shared with OnEventQuarterNotifyReq: world-chat announce line.
boost::asio::awaitable<void>
BroadcastEventQuarterNotify(const HandlerContext& ctx,
                            const std::string& announce)
{
    if (!ctx.peers) co_return;
    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwChatReq(p, 0, 0, 0, 0, std::string{},
            kCountryNeutral, kCountryNeutral, chat::kWorld,
            chat::kWorld, 0, announce);
    co_return;
}

} // namespace

boost::asio::awaitable<void>
RunEventQuarterTick(const HandlerContext& ctx)
{
    if (!ctx.evqt || !ctx.peers)
        co_return;
    const auto now =
        static_cast<std::int64_t>(std::time(nullptr));
    const auto actions = ctx.evqt->Tick(now);
    if (actions.announce)
        co_await BroadcastEventQuarterNotify(ctx, *actions.announce);
    if (actions.present)
        co_await BroadcastEventQuarterPresent(ctx,
            actions.present->day, actions.present->hour,
            actions.present->minute, actions.present->present);
    co_return;
}

boost::asio::awaitable<void>
DispatchExpiredEntry(const HandlerContext& ctx,
                     const ExpiredEntry& entry)
{
    switch (entry.type)
    {
    case kExpiredGuildWanted:
    {
        // Legacy: DelGuildWanted + DM_GUILDWANTEDDEL (SSHandler.cpp:
        // 10712) - same actions as the W3a-19 sweep.
        if (ctx.guild_wanted)
            ctx.guild_wanted->Remove(entry.value1);
        if (ctx.guild_repo)
            co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                [repo = ctx.guild_repo, gid = entry.value1]
                { repo->DeleteWanted(gid); });
        break;
    }
    case kExpiredTacticsWanted:
    {
        // Legacy: DelGuildTacticsWanted + the DM del. The tactics-
        // wanted DB table is the deferred guild-extras item, so the
        // registry mutation is the whole action here.
        if (ctx.guild_tactics_wanted)
            ctx.guild_tactics_wanted->Remove(entry.value1,
                entry.value2);
        break;
    }
    case kExpiredTactics:
    {
        // Legacy: GuildTacticsDel(guild, member, 3) - targeted
        // contract end (the W3a-36 sweep does the same by deadline).
        if (!ctx.guilds)
            break;
        auto guild = ctx.guilds->Find(entry.value1);
        if (!guild)
            break;
        bool ended = false;
        {
            std::lock_guard g(guild->lock);
            auto& roster = guild->tactics_members;
            for (auto it = roster.begin(); it != roster.end(); ++it)
                if (it->id == entry.value2)
                {
                    roster.erase(it);
                    ended = true;
                    break;
                }
        }
        if (ended && ctx.chars)
            if (auto c = ctx.chars->Find(entry.value2))
            {
                std::lock_guard g(c->lock);
                if (c->tactics_guild_id == entry.value1)
                    c->tactics_guild_id = 0;
            }
        break;
    }
    default:
        break;                          // legacy default: ignore
    }
    co_return;
}

boost::asio::awaitable<void>
RunExpiredSweep(const HandlerContext& ctx)
{
    if (!ctx.expired)
        co_return;
    const auto now =
        static_cast<std::int64_t>(std::time(nullptr));
    for (const auto& entry : ctx.expired->PopDue(now))
        co_await DispatchExpiredEntry(ctx, entry);
    co_return;
}

boost::asio::awaitable<void>
OnSmEventExpiredReq(std::shared_ptr<PeerSession> peer,
                    std::vector<std::byte>       body,
                    const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.expired)
    {
        spdlog::warn("OnSmEventExpiredReq[{}]: expired buffer not "
                     "wired", ip);
        co_return;
    }
    wire::Reader r(body.data(), body.size());
    std::uint8_t insert = 0;
    ExpiredEntry e{};
    if (!r.Read(insert) || !r.Read(e.type) || !r.Read(e.time) ||
        !r.Read(e.value1) || !r.Read(e.value2))
    {
        spdlog::warn("OnSmEventExpiredReq[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }
    ctx.expired->InsertOrRemove(insert != 0, e);
    co_return;
}

boost::asio::awaitable<void>
OnSmEventExpiredAck(std::shared_ptr<PeerSession> peer,
                    std::vector<std::byte>       body,
                    const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    wire::Reader r(body.data(), body.size());
    ExpiredEntry e{};
    if (!r.Read(e.type) || !r.Read(e.time) || !r.Read(e.value1) ||
        !r.Read(e.value2))
    {
        spdlog::warn("OnSmEventExpiredAck[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }
    co_await DispatchExpiredEntry(ctx, e);
    co_return;
}

} // namespace tworldsvr::handlers
