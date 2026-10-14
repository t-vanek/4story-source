// Entry point for the modernized TWorldSvrAsio binary.
//
// W2 (this revision) adds:
//   - SessionPool + DB worker thread_pool when [database] is set
//     (used by W3+ guild / friend / soulmate SOCI handlers)
//   - CharRegistry — the partitioned in-memory char index that
//     replaces the legacy `m_mapTCHAR` single-lock map. Closes
//     the in-memory half of PATCH_README §6 W-2.
//   - First batch of char-lifecycle handlers
//     (MW_ADDCHAR_ACK, MW_CLOSECHAR_ACK)
//
// W3+ layers on guild / party / corps services + the peer dialer
// that lets handlers send replies back to the right map server.
// See Server/TWorldSvrAsio/README.md for the phase plan.

#include "config.h"
#include "db/schema_validator.h"
#include "services/cash_item_sale_registry.h"
#include "services/char_registry.h"
#include "services/ctrl_svr_slot.h"
#include "services/event_registry.h"
#include "services/guild_level_cache.h"
#include "services/server_messages.h"
#include "services/postgresql_chat_repository.h"
#include "services/guild_registry.h"
#include "services/mapper_profiles.h"
#include "services/guild_wanted_registry.h"
#include "services/guild_wanted_sweep.h"
#include "services/guild_tactics_wanted_registry.h"
#include "services/party_registry.h"
#include "services/corps_registry.h"
#include "services/tms_registry.h"
#include "services/guild_tactics_sweep.h"
#include "services/peer_registry.h"
#include "services/soci_guild_level_repository.h"
#include "services/postgresql_guild_level_repository.h"
#include "services/soci_guild_repository.h"
#include "services/soci_friend_repository.h"
#include "services/postgresql_friend_repository.h"
#include "services/postgresql_soulmate_offers.h"
#include "services/postgresql_world_owner.h"
#include "services/postgresql_reserved_mail.h"
#include "fourstory/db/co_offload.h"
#include "services/soci_item_state_repository.h"
#include "services/soci_cash_sale_repository.h"
#include "services/soci_service_ops_repository.h"
#include "services/soci_month_rank_repository.h"
#include "services/soci_war_ops_repository.h"
#include "services/soci_cmgift_repository.h"
#include "services/soci_lucky_event_repository.h"
#include "services/soci_rps_repository.h"
#include "services/soci_bow_repository.h"
#include "services/soci_tournament_repository.h"
#include "services/tournament_boot.h"
#include "services/war_country_index.h"
#include "world_server.h"

#include "fourstory/db/session_pool.h"
#include "fourstory/ops/health_endpoint.h"
#include "fourstory/ops/registry_refresher.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <atomic>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/thread_pool.hpp>

#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>

namespace {
void Usage()
{
    std::printf(
        "tworldsvr_asio — modernized 4Story cluster coordinator (W2)\n"
        "Usage: tworldsvr_asio [--config FILE]\n");
}
} // namespace

int main(int argc, char** argv)
{
    std::string config_path = "tworldsvr.toml";
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc)
            config_path = argv[++i];
        else if (std::strcmp(argv[i], "--help") == 0 ||
                 std::strcmp(argv[i], "-h") == 0)
        {
            Usage();
            return 0;
        }
    }

    try
    {
        auto cfg = tworldsvr::LoadConfig(config_path);
        spdlog::set_level(cfg.log_level);
        if(cfg.database.connection_string.empty()||cfg.database.backend.empty())
            throw std::runtime_error("World requires an explicit database configuration");
        if(!cfg.database.worker_threads)
            throw std::runtime_error("World requires database.worker_threads>0");

        boost::asio::io_context io;

        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&io](auto, int sig) {
            spdlog::info("received signal {}, shutting down", sig);
            io.stop();
        });

        // --- Object-mapper profiles -------------------------------
        //
        // Configure the fourstory::mapper Automapper once at startup
        // (mirrors TControlSvrAsio). GuildMappingProfile wires
        // GuildRow→TGuild / GuildMemberRow→TGuildMember and
        // FriendMappingProfile wires Friend{Forward,Reverse}Row→FriendRow,
        // so the SOCI repositories' read paths map DB rows to domain
        // objects without hand-written field copies. Registered
        // unconditionally — the
        // Fake repositories don't use it, but keeping the bootstrap
        // identical on both paths avoids a "works only with a DB" trap.
        {
            auto& reg = fourstory::mapper::MapperRegistry::Get();
            if (!reg.Applied())
            {
                reg.Register<tworldsvr::GuildMappingProfile>();
                reg.Register<tworldsvr::FriendMappingProfile>();
                reg.ApplyAll();
                spdlog::info("mapper: {} profile(s) applied",
                    reg.Count());
            }
        }

        // --- DB infrastructure (W2 + W3a-1) -----------------------
        //
        // Pool sizing follows TLoginSvrAsio / TControlSvrAsio: N
        // SOCI sessions + a worker thread_pool that handlers offload
        // synchronous SOCI calls onto via fourstory::db::CoOffloadIf.
        // The production executable requires this infrastructure. Fake
        // repositories remain available only to the existing test targets.
        std::unique_ptr<tworldsvr::PostgreSQLWorldOwner> world_owner;
        std::unique_ptr<fourstory::db::SessionPool>      db_pool_owner;
        std::unique_ptr<boost::asio::thread_pool>        worker_pool;
        std::unique_ptr<tworldsvr::IGuildRepository>     guild_repo;
        std::unique_ptr<tworldsvr::IGuildLevelRepository> guild_level_repo;
        std::unique_ptr<tworldsvr::IFriendRepository>    friend_repo;
        std::unique_ptr<tworldsvr::PostgreSQLSoulmateOffers> soulmate_offers;
        tworldsvr::ServerMessages server_messages;
        std::vector<std::uint32_t> operators;
        std::unique_ptr<tworldsvr::PostgreSQLChatRepository> chat_repo;
        std::atomic<bool> friend_persistence_failed{false};
        std::unique_ptr<tworldsvr::IItemStateRepository> item_state_repo;
        std::unique_ptr<tworldsvr::ICashSaleRepository>  cash_sale_repo;
        std::unique_ptr<tworldsvr::IServiceOpsRepository> service_ops_repo;
        std::unique_ptr<tworldsvr::IMonthRankRepository> month_rank_repo;
        std::unique_ptr<tworldsvr::IWarOpsRepository>    war_ops_repo;
        std::unique_ptr<tworldsvr::ICmGiftRepository>    cmgift_repo;
        std::unique_ptr<tworldsvr::ILuckyEventRepository> lucky_repo;
        std::unique_ptr<tworldsvr::IRpsRepository>       rps_repo;
        std::unique_ptr<tworldsvr::ITournamentRepository> tournament_repo;
        std::unique_ptr<tworldsvr::IBowRepository>       bow_repo;
        std::unique_ptr<tworldsvr::PostgreSQLReservedMail> reserved_mail;

        if (!cfg.database.connection_string.empty())
        {
            if (cfg.database.backend.empty())
                throw std::runtime_error(
                    "database.connection_string set but database.backend empty");
            const auto backend =
                fourstory::db::ParseBackend(cfg.database.backend);
            db_pool_owner = std::make_unique<fourstory::db::SessionPool>(
                backend, cfg.database.connection_string,
                cfg.database.pool_size);

            if (cfg.database.worker_threads > 0)
            {
                worker_pool = std::make_unique<boost::asio::thread_pool>(
                    cfg.database.worker_threads);
                spdlog::info("db worker pool: {} threads",
                    cfg.database.worker_threads);
            }
            else
            {
                spdlog::info("db worker pool: disabled "
                             "(SOCI runs in-line on io_context — dev only)");
            }
            spdlog::info("db: {} pool_size={} ready",
                fourstory::db::BackendName(backend), cfg.database.pool_size);

            if(backend==fourstory::db::Backend::PostgreSQL) {
                if(!worker_pool)throw std::runtime_error("Native World mail requires database.worker_threads>0");
                world_owner=std::make_unique<tworldsvr::PostgreSQLWorldOwner>(cfg.database.connection_string,cfg.group_id);
                friend_repo=std::make_unique<tworldsvr::PostgreSQLFriendRepository>(*db_pool_owner,cfg.group_id,
                    world_owner->Token(),cfg.native_world_client_text_encoding,[&io,&friend_persistence_failed] {
                        if(!friend_persistence_failed.exchange(true))
                            spdlog::critical("native social persistence failed; World stopped for reconciliation");
                        io.stop();
                    });
                reserved_mail=std::make_unique<tworldsvr::PostgreSQLReservedMail>(*db_pool_owner,cfg.group_id,world_owner->Token());
                soulmate_offers=std::make_unique<tworldsvr::PostgreSQLSoulmateOffers>(
                    static_cast<tworldsvr::PostgreSQLFriendRepository&>(*friend_repo));
                server_messages=tworldsvr::LoadPostgreSQLServerMessages(*db_pool_owner,cfg.group_id,
                    world_owner->Token(),cfg.native_world_client_text_encoding);
                chat_repo=std::make_unique<tworldsvr::PostgreSQLChatRepository>(
                    static_cast<tworldsvr::PostgreSQLFriendRepository&>(*friend_repo));
                operators=chat_repo->LoadOperators();
            }

            // Fail-fast on missing TGUILD* columns before the
            // listener accepts traffic.
            tworldsvr::db::ValidateWorldSchema(*db_pool_owner);
            guild_repo = std::make_unique<tworldsvr::SociGuildRepository>(
                *db_pool_owner);
            if(world_owner)guild_level_repo=std::make_unique<tworldsvr::PostgreSQLGuildLevelRepository>(
                *db_pool_owner,cfg.group_id,world_owner->Token());
            else guild_level_repo=std::make_unique<tworldsvr::SociGuildLevelRepository>(*db_pool_owner);
            if(!friend_repo)friend_repo = std::make_unique<tworldsvr::SociFriendRepository>(
                *db_pool_owner,[&io,&friend_persistence_failed] {
                    if(!friend_persistence_failed.exchange(true))
                        spdlog::critical("social persistence failed; World stopped for reconciliation");
                    io.stop();
                });
            item_state_repo =
                std::make_unique<tworldsvr::SociItemStateRepository>(
                    *db_pool_owner);
            cash_sale_repo =
                std::make_unique<tworldsvr::SociCashSaleRepository>(
                    *db_pool_owner);
            service_ops_repo =
                std::make_unique<tworldsvr::SociServiceOpsRepository>(
                    *db_pool_owner);
            month_rank_repo =
                std::make_unique<tworldsvr::SociMonthRankRepository>(
                    *db_pool_owner);
            war_ops_repo =
                std::make_unique<tworldsvr::SociWarOpsRepository>(
                    *db_pool_owner);
            cmgift_repo =
                std::make_unique<tworldsvr::SociCmGiftRepository>(
                    *db_pool_owner);
            lucky_repo =
                std::make_unique<tworldsvr::SociLuckyEventRepository>(
                    *db_pool_owner);
            rps_repo =
                std::make_unique<tworldsvr::SociRpsRepository>(
                    *db_pool_owner);
            tournament_repo =
                std::make_unique<tworldsvr::SociTournamentRepository>(
                    *db_pool_owner);
            bow_repo =
                std::make_unique<tworldsvr::SociBowRepository>(
                    *db_pool_owner);
        }

        // --- Char + guild registries ------------------------------
        //
        // Both use the same 16-shard partitioning + per-entry mutex
        // model. The char half closes W-2's in-memory bottleneck;
        // the guild half closes the m_mapTGuild bottleneck the same
        // way. Both live for the process lifetime.
        tworldsvr::CharRegistry  chars;
        tworldsvr::GuildRegistry guilds;
        tworldsvr::PeerRegistry  peers;
        tworldsvr::GuildLevelCache    guild_levels;
        tworldsvr::GuildWantedRegistry guild_wanted;
        tworldsvr::GuildTacticsWantedRegistry guild_tactics_wanted;
        tworldsvr::PartyRegistry party;
        tworldsvr::CorpsRegistry corps;
        tworldsvr::TmsRegistry   tms;
        tworldsvr::BowRegistry   bow;
        tworldsvr::BrRegistry    br;
        tworldsvr::RpsRegistry   rps;
        tworldsvr::EventRegistry events;
        tworldsvr::CashItemSaleRegistry cash_sales;
        tworldsvr::CtrlSvrSlot          ctrl_svr;
        tworldsvr::MonthRankRegistry    month_rank;
        tworldsvr::WarCountryIndex      war_index;
        tworldsvr::CastleWarRegistry    castle_war;
        tworldsvr::CmGiftRegistry       cmgifts;
        tworldsvr::EventQuarterScheduler evqt;
        tworldsvr::ExpiredBuffer        expired;
        tworldsvr::TournamentRegistry   tournaments;
        tworldsvr::CtrlSvrSlot          bow_server;
        if (cmgift_repo)
        {
            cmgifts.LoadFrom(cmgift_repo->LoadChart());
            spdlog::info("cmgift catalogue: {} gift(s) loaded",
                cmgifts.Size());
        }
        if (lucky_repo)
        {
            std::vector<tworldsvr::EventQuarterEntry> rows;
            for (const auto& e : lucky_repo->ListAll())
            {
                tworldsvr::EventQuarterEntry q{};
                q.id = e.id; q.day = e.day; q.hour = e.hour;
                q.minute = e.minute;
                q.present = e.present; q.announce = e.announce;
                rows.push_back(std::move(q));
            }
            evqt.LoadFrom(rows,
                static_cast<std::int64_t>(std::time(nullptr)));
            spdlog::info("event-quarter scheduler: {} entrie(s) "
                         "loaded", evqt.Size());
        }
        if (rps_repo)
        {
            std::size_t dates = 0;
            for (const auto& g : rps_repo->LoadGames())
                rps.Insert(g);
            for (const auto& d : rps_repo->LoadWinDates())
                if (rps.HydrateWinDate(d.type, d.win_count, d.date_unix))
                    ++dates;
            spdlog::info("rps games: {} config row(s), {} win date(s) "
                         "loaded", rps.Size(), dates);
        }
        if (bow_repo)
        {
            // W6-54: TBOWSETTINGSCHART config + TCUSTOMTIMECHART
            // BT_BOW start times (TWorldSvr.cpp:1234-1290).
            if (const auto bs = bow_repo->LoadSettings())
            {
                bow.Configure(bs->map_id, bs->min_players,
                    bs->max_nation_difference, bs->alarm_dur,
                    bs->buy_dur, bs->battle_dur);
                for (const auto t : bow_repo->LoadStartTimes())
                    bow.AddStartTime(t);
                std::time_t bnow = std::time(nullptr);
                std::tm     blt{};
#ifdef _WIN32
                localtime_s(&blt, &bnow);
#else
                localtime_r(&bnow, &blt);
#endif
                bow.Init(static_cast<std::uint32_t>(
                    blt.tm_hour * 3600 + blt.tm_min * 60 +
                    blt.tm_sec));
                spdlog::info("bow battleground: configured "
                             "(map {}, next start {}s)",
                    bs->map_id, bow.NextStart());
            }
        }
        if (tournament_repo)
        {
            // W6-50/52: main tournament + operator lucky events +
            // mid-flight resume incl. the player reload
            // (TWorldSvr.cpp:1574-1889). The first-grade predicate
            // reads the fame array (empty until the first month
            // rollover — the port's month-rank design decision).
            tworldsvr::LoadTournamentState(tournaments, *tournament_repo,
                static_cast<std::int64_t>(std::time(nullptr)),
                [&month_rank, &tournaments](std::uint8_t country,
                                            std::uint32_t char_id) {
                    if (country >= tworldsvr::kMonthRankCountryCount)
                        return false;
                    const auto group = month_rank.FirstGradeGroup();
                    const std::uint8_t count =
                        tournaments.FirstGroupCount();
                    for (std::uint8_t j = 1; j < count &&
                         j < tworldsvr::kFirstGradeGroupCount; ++j)
                        if (group[country][j].char_id == char_id)
                            return true;
                    return false;
                });
        }
        {
            // Legacy TWorldSvr.cpp:1894 - the rank month boots from
            // the local clock; the table itself starts empty and
            // fills from live MW_MONTHRANKUPDATE_ACK reports.
            std::time_t now = std::time(nullptr);
            std::tm     lt{};
#ifdef _WIN32
            localtime_s(&lt, &now);
#else
            localtime_r(&now, &lt);
#endif
            month_rank.SetRankMonth(
                static_cast<std::uint8_t>(lt.tm_mon + 1));
        }

        // Warm the guild-level chart from the backing store. Empty
        // SOCI returns every TGUILDCHART row. Handlers consult Find()
        // for per-level caps (CheckPeerage gate, AddMember cap,
        // cabinet slot count). nullptr-tolerant.
        if (guild_level_repo)
        {
            guild_levels.LoadFrom(guild_level_repo->LoadAll());
            spdlog::info("guild_levels: {} chart row(s) cached",
                guild_levels.Size());
        }

        // Warm the guild cache from the required backing store: every
        // non-disbanded TGUILDTABLE row.
        if (guild_repo)
        {
            const auto preloaded = guild_repo->LoadAll();
            for (auto& g : preloaded)
                guilds.Insert(g);
            spdlog::info("guild registry: {} guild(s) preloaded",
                guilds.Size());
        }

        fourstory::security::IpAllowlist control_ip_allowlist;
        if(control_ip_allowlist.AddAll(cfg.control_ip_allowlist)!=cfg.control_ip_allowlist.size())
            throw std::runtime_error("Invalid Control transport allowlist");
        tworldsvr::HandlerContext ctx{};
        ctx.control_ip_allowlist=&control_ip_allowlist;
        ctx.io           = &io;
        ctx.db_pool      = worker_pool.get();
        ctx.chars        = &chars;
        ctx.guilds       = &guilds;
        ctx.peers        = &peers;
        ctx.guild_repo   = guild_repo.get();
        ctx.friend_repo  = friend_repo.get();
        ctx.soulmate_offers = soulmate_offers.get();
        ctx.guild_levels = &guild_levels;
        ctx.server_messages = &server_messages;
        ctx.operators = &operators;
        ctx.chat_repo = chat_repo.get();
        ctx.guild_wanted = &guild_wanted;
        ctx.guild_tactics_wanted = &guild_tactics_wanted;
        ctx.parties      = &party;
        ctx.corps        = &corps;
        ctx.tms          = &tms;
        ctx.bow          = &bow;
        ctx.br           = &br;
        ctx.rps          = &rps;
        ctx.rps_repo     = rps_repo.get();
        ctx.events       = &events;
        ctx.cash_sales   = &cash_sales;
        ctx.ctrl_svr     = &ctrl_svr;
        ctx.item_state_repo = item_state_repo.get();
        ctx.cash_sale_repo  = cash_sale_repo.get();
        ctx.service_ops     = service_ops_repo.get();
        ctx.month_rank      = &month_rank;
        ctx.month_rank_repo = month_rank_repo.get();
        ctx.war_index       = &war_index;
        ctx.war_ops         = war_ops_repo.get();
        ctx.castle_war      = &castle_war;
        ctx.cmgifts         = &cmgifts;
        ctx.cmgift_repo     = cmgift_repo.get();
        ctx.lucky_repo      = lucky_repo.get();
        ctx.evqt            = &evqt;
        ctx.expired         = &expired;
        ctx.tournaments     = &tournaments;
        ctx.tournament_repo = tournament_repo.get();
        ctx.bow_repo        = bow_repo.get();
        ctx.bow_server      = &bow_server;
        ctx.castle_war_day  = cfg.castle_war_day;
        ctx.group_id        = cfg.group_id;
        ctx.nation       = cfg.nation;

        tworldsvr::WorldServerConfig svr_cfg{};
        svr_cfg.port            = cfg.port;
        svr_cfg.max_connections = cfg.max_connections;
        svr_cfg.ctx             = ctx;

        tworldsvr::WorldServer server(io, svr_cfg);
        spdlog::info("world server listening on 0.0.0.0:{} (max_connections={})",
            server.Port(), cfg.max_connections);
        boost::asio::co_spawn(io, server.Run(), boost::asio::detached);

        std::unique_ptr<fourstory::ops::HealthEndpoint> health;
        if (cfg.health_port != 0)
        {
            try
            {
                health = std::make_unique<fourstory::ops::HealthEndpoint>(
                    io, cfg.health_port);
                spdlog::info("health endpoint listening on 0.0.0.0:{}",
                    health->Port());
                boost::asio::co_spawn(io, health->Run(),
                    boost::asio::detached);
            }
            catch (const std::exception& ex)
            {
                spdlog::warn("health endpoint failed to bind on port {}: {}",
                    cfg.health_port, ex.what());
            }
        }

        // W3a-19: periodic wanted-board expiry sweep. Closes the
        // legacy SM_EVENTEXPIRED_ACK fan-out path
        // (TWorldSvr.cpp:5280) without the SP/timer service
        // dependency — the prune runs in-process on a RegistryRefresher
        // tick. period_sec=0 disables (test-only).
        std::shared_ptr<fourstory::ops::RegistryRefresher> wanted_sweeper;
        if (cfg.wanted_sweep_period_sec != 0)
        {
            wanted_sweeper = fourstory::ops::RegistryRefresher::Make(
                io, std::chrono::seconds(cfg.wanted_sweep_period_sec));
            wanted_sweeper->AddCoroutineHook(
                [&guild_wanted, guild_repo_ptr = guild_repo.get(),
                 db_pool = worker_pool.get()]()
                    -> boost::asio::awaitable<void> {
                    co_await tworldsvr::SweepExpiredWanted(
                        guild_wanted, guild_repo_ptr, db_pool);
                });
            wanted_sweeper->Start();
            spdlog::info("wanted-board expiry sweep enabled "
                         "(period={}s)",
                cfg.wanted_sweep_period_sec);
        }

        // W6-42: month-boundary rollover check (legacy world timer,
        // TWorldSvr.cpp:4065 - posts SM_MONTHRANKSAVE to itself when
        // the local month leaves m_bRankMonth). period_sec=0 disables.
        std::shared_ptr<fourstory::ops::RegistryRefresher> month_sweeper;
        if (cfg.month_rollover_check_period_sec != 0)
        {
            month_sweeper = fourstory::ops::RegistryRefresher::Make(
                io, std::chrono::seconds(
                        cfg.month_rollover_check_period_sec));
            month_sweeper->AddCoroutineHook(
                [ctx]() -> boost::asio::awaitable<void> {
                    std::time_t now = std::time(nullptr);
                    std::tm     lt{};
#ifdef _WIN32
                    localtime_s(&lt, &now);
#else
                    localtime_r(&now, &lt);
#endif
                    const auto cur =
                        static_cast<std::uint8_t>(lt.tm_mon + 1);
                    if (ctx.month_rank &&
                        cur != ctx.month_rank->RankMonth())
                        co_await tworldsvr::handlers::
                            RunMonthRankRollover(ctx);
                });
            month_sweeper->Start();
            spdlog::info("month-rollover check enabled (period={}s)",
                cfg.month_rollover_check_period_sec);
        }

        std::shared_ptr<fourstory::ops::RegistryRefresher> world_owner_sweeper;
        if(world_owner) {
            world_owner_sweeper=fourstory::ops::RegistryRefresher::Make(io,std::chrono::seconds(1));
            world_owner_sweeper->AddCoroutineHook(
                [&io,&friend_persistence_failed,owner=world_owner.get(),workers=worker_pool.get()]()
                    ->boost::asio::awaitable<void> {
                    const auto healthy=co_await fourstory::db::CoOffloadIf(workers,[owner]{return owner->Healthy();});
                    if(!healthy) {
                        if(!friend_persistence_failed.exchange(true))
                            spdlog::critical("native World ownership lost; stopped without reclaim or write replay");
                        io.stop();
                    }
                });
            world_owner_sweeper->Start();
        }

        std::int64_t soulmate_cursor=0;
        std::shared_ptr<fourstory::ops::RegistryRefresher> soulmate_sweeper;
        if(soulmate_offers) {
            soulmate_sweeper=fourstory::ops::RegistryRefresher::Make(io,std::chrono::seconds(1));
            soulmate_sweeper->AddCoroutineHook([ctx,&soulmate_cursor,&io,&friend_persistence_failed]()
                ->boost::asio::awaitable<void> {
                try {co_await tworldsvr::handlers::RunSoulmateMaintenance(ctx,soulmate_cursor);}
                catch(...) {
                    friend_persistence_failed.store(true);
                    spdlog::critical("native soulmate reconciliation failed; World stopped without replay");
                    io.stop();
                }
            });
            soulmate_sweeper->Start();
        }

        // W6-43: war-country index - boot load + periodic refresh
        // (legacy: boot init at TWorldSvr.cpp:820 + the day-change
        // DM_ACTIVECHARUPDATE round-trip). period_sec=0 disables.
        if (war_ops_repo)
        {
            boost::asio::co_spawn(io,
                tworldsvr::handlers::RefreshWarCountryIndex(ctx),
                boost::asio::detached);
        }
        std::shared_ptr<fourstory::ops::RegistryRefresher> war_index_sweeper;
        if (cfg.war_index_refresh_period_sec != 0 && war_ops_repo)
        {
            war_index_sweeper = fourstory::ops::RegistryRefresher::Make(
                io, std::chrono::seconds(
                        cfg.war_index_refresh_period_sec));
            war_index_sweeper->AddCoroutineHook(
                [ctx]() -> boost::asio::awaitable<void> {
                    co_await tworldsvr::handlers::
                        RefreshWarCountryIndex(ctx);
                });
            war_index_sweeper->Start();
            spdlog::info("war-country index refresh enabled "
                         "(period={}s)",
                cfg.war_index_refresh_period_sec);
        }

        // W6-48: lucky-event scheduler tick + expiry-queue pop
        // (legacy CheckEventQuarter / CheckEventExpired timer work).
        // period_sec=0 disables.
        std::shared_ptr<fourstory::ops::RegistryRefresher> evqt_sweeper;
        if (cfg.event_quarter_check_period_sec != 0)
        {
            evqt_sweeper = fourstory::ops::RegistryRefresher::Make(
                io, std::chrono::seconds(
                        cfg.event_quarter_check_period_sec));
            evqt_sweeper->AddCoroutineHook(
                [ctx]() -> boost::asio::awaitable<void> {
                    co_await tworldsvr::handlers::RunEventQuarterTick(
                        ctx);
                    co_await tworldsvr::handlers::RunExpiredSweep(ctx);
                });
            evqt_sweeper->Start();
            spdlog::info("event-quarter/expiry sweeper enabled "
                         "(period={}s)",
                cfg.event_quarter_check_period_sec);
        }

        // W6-54: Bow battleground window tick (legacy per-second walk,
        // TWorldSvr.cpp:4094). period_sec=0 disables.
        std::shared_ptr<fourstory::ops::RegistryRefresher> bow_sweeper;
        if (cfg.bow_check_period_sec != 0)
        {
            bow_sweeper = fourstory::ops::RegistryRefresher::Make(
                io, std::chrono::seconds(cfg.bow_check_period_sec));
            bow_sweeper->AddCoroutineHook(
                [ctx]() -> boost::asio::awaitable<void> {
                    std::time_t bnow = std::time(nullptr);
                    std::tm     blt{};
#ifdef _WIN32
                    localtime_s(&blt, &bnow);
#else
                    localtime_r(&bnow, &blt);
#endif
                    co_await tworldsvr::handlers::RunBowTick(ctx,
                        static_cast<std::uint32_t>(
                            blt.tm_hour * 3600 + blt.tm_min * 60 +
                            blt.tm_sec),
                        static_cast<std::uint64_t>(
                            std::chrono::duration_cast<
                                std::chrono::milliseconds>(
                                std::chrono::steady_clock::now()
                                    .time_since_epoch())
                                .count()));
                });
            bow_sweeper->Start();
            spdlog::info("bow tick sweeper enabled (period={}s)",
                cfg.bow_check_period_sec);
        }

        // W6-51: tournament step-advance tick (legacy per-second
        // walk over the active schedule, TWorldSvr.cpp:4012).
        // period_sec=0 disables.
        std::shared_ptr<fourstory::ops::RegistryRefresher>
            tournament_sweeper;
        if (cfg.tournament_check_period_sec != 0)
        {
            tournament_sweeper = fourstory::ops::RegistryRefresher::Make(
                io, std::chrono::seconds(
                        cfg.tournament_check_period_sec));
            tournament_sweeper->AddCoroutineHook(
                [ctx]() -> boost::asio::awaitable<void> {
                    co_await tworldsvr::handlers::RunTournamentTick(
                        ctx,
                        static_cast<std::int64_t>(std::time(nullptr)));
                });
            tournament_sweeper->Start();
            spdlog::info("tournament tick sweeper enabled "
                         "(period={}s)",
                cfg.tournament_check_period_sec);
        }

        // W3a-36: periodic tactics-contract expiry sweep. Ends
        // tactics-member contracts whose end_time has elapsed
        // (legacy EXPIRED_GT path) without the timer-service
        // dependency. period_sec=0 disables.
        std::shared_ptr<fourstory::ops::RegistryRefresher> tactics_sweeper;
        if (cfg.tactics_sweep_period_sec != 0)
        {
            tactics_sweeper = fourstory::ops::RegistryRefresher::Make(
                io, std::chrono::seconds(cfg.tactics_sweep_period_sec));
            tactics_sweeper->AddCoroutineHook(
                [&guilds, &chars]() -> boost::asio::awaitable<void> {
                    co_await tworldsvr::SweepExpiredTactics(guilds, &chars);
                });
            tactics_sweeper->Start();
            spdlog::info("tactics-contract expiry sweep enabled "
                         "(period={}s)",
                cfg.tactics_sweep_period_sec);
        }

        // Original OnTimer posts RESERVEDPOSTSEND on minute change. The native
        // durable minute gate and queue status serialize duplicate World ticks;
        // Map's existing fenced notification poll publishes original POSTRECV.
        std::shared_ptr<fourstory::ops::RegistryRefresher> reserved_mail_sweeper;
        bool reserved_mail_failed=false;
        if(reserved_mail) {
            reserved_mail_sweeper=fourstory::ops::RegistryRefresher::Make(io,std::chrono::seconds(1));
            reserved_mail_sweeper->AddCoroutineHook([&io,&reserved_mail_failed,mail=reserved_mail.get(),workers=worker_pool.get()]()
                ->boost::asio::awaitable<void> {
                const auto now=static_cast<std::int64_t>(std::time(nullptr));
                try {
                    const auto result=co_await fourstory::db::CoOffload(*workers,[mail,now]{return mail->Deliver(now);});
                    if(result.delivered||result.missing)spdlog::info("reserved mail: delivered={}, missing={}",result.delivered,result.missing);
                    const auto bills=co_await fourstory::db::CoOffload(*workers,[mail,now]{return mail->ReturnOfflineBills(now);});
                    if(bills.returned)spdlog::info("offline bills: returned={}",bills.returned);
                    if(bills.missing_senders)spdlog::warn("offline bills retained for missing senders: {}",bills.missing_senders);
                }catch(...) {
                    // Credential/source-safe failure. No timer replay of a
                    // write with unknown outcome and no fabricated empty queue.
                    spdlog::critical("native mail worker failed; World stopped for reconciliation");
                    reserved_mail_failed=true;
                    io.stop();
                }
            });
            reserved_mail_sweeper->Start();
        }

        io.run();

        if (wanted_sweeper) wanted_sweeper->Stop();
        if (tactics_sweeper) tactics_sweeper->Stop();
        if (reserved_mail_sweeper) reserved_mail_sweeper->Stop();
        if(world_owner_sweeper)world_owner_sweeper->Stop();
        if(soulmate_sweeper)soulmate_sweeper->Stop();

        // Drain in-flight SOCI work before tearing the pool down so
        // a query doesn't keep a session reference alive past the
        // pool's destructor.
        if (worker_pool)
        {
            worker_pool->stop();
            worker_pool->join();
        }
        if(reserved_mail_failed||friend_persistence_failed.load())return 1;
    }
    catch (const std::exception& ex)
    {
        spdlog::critical("fatal: {}", ex.what());
        return 1;
    }
    return 0;
}
