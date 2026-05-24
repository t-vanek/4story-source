# TWorldSvrAsio — modernized 4Story cluster coordinator.
#
# W1 ships the scaffold: TOML config, accept loop, packet framing,
# dispatch stub (logs + drops every wID). W2 adds the char SOCI
# repository + first batch of CHAR / USER / OnRW handlers. See
# Server/TWorldSvrAsio/README.md §4 for the full W2..W7 rollout.

set(TWORLD_ASIO_SRC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(spdlog QUIET)
find_package(tomlplusplus QUIET)
find_package(SOCI CONFIG QUIET)

# SOCI link list — needed transitively through fourstory_common even
# though W1 doesn't open any DB session itself.
set(TWORLD_SOCI_LIBS "")
if(SOCI_FOUND)
    if(TARGET SOCI::soci_core)
        list(APPEND TWORLD_SOCI_LIBS SOCI::soci_core)
    elseif(TARGET SOCI::soci_core_static)
        list(APPEND TWORLD_SOCI_LIBS SOCI::soci_core_static)
    endif()
    if(TARGET SOCI::soci_odbc)
        list(APPEND TWORLD_SOCI_LIBS SOCI::soci_odbc)
    elseif(TARGET SOCI::soci_odbc_static)
        list(APPEND TWORLD_SOCI_LIBS SOCI::soci_odbc_static)
    endif()
else()
    list(APPEND TWORLD_SOCI_LIBS soci_odbc soci_core dl)
endif()

# Core library — shared between the binary and the test executable
# so tests link against the exact same TUs without rebuilding.
add_library(tworldsvr_asio_core STATIC
    ${TWORLD_ASIO_SRC}/config.cpp
    ${TWORLD_ASIO_SRC}/world_session.cpp
    ${TWORLD_ASIO_SRC}/world_server.cpp
    ${TWORLD_ASIO_SRC}/db/schema_validator.cpp
    ${TWORLD_ASIO_SRC}/handlers/dispatch.cpp
    ${TWORLD_ASIO_SRC}/handlers/handlers_char.cpp
    ${TWORLD_ASIO_SRC}/handlers/handlers_char_base.cpp
    ${TWORLD_ASIO_SRC}/handlers/handlers_guild.cpp
    ${TWORLD_ASIO_SRC}/handlers/handlers_party.cpp
    ${TWORLD_ASIO_SRC}/handlers/handlers_corps.cpp
    ${TWORLD_ASIO_SRC}/handlers/handlers_friend.cpp
    ${TWORLD_ASIO_SRC}/handlers/handlers_chat.cpp
    ${TWORLD_ASIO_SRC}/handlers/handlers_soulmate.cpp
    ${TWORLD_ASIO_SRC}/handlers/handlers_tms.cpp
    ${TWORLD_ASIO_SRC}/handlers/handlers_post.cpp
    ${TWORLD_ASIO_SRC}/handlers/handlers_occupy.cpp
    ${TWORLD_ASIO_SRC}/handlers/handlers_event.cpp
    ${TWORLD_ASIO_SRC}/handlers/handlers_combat.cpp
    ${TWORLD_ASIO_SRC}/handlers/handlers_item.cpp
    ${TWORLD_ASIO_SRC}/handlers/handlers_admin.cpp
    ${TWORLD_ASIO_SRC}/handlers/handlers_rank.cpp
    ${TWORLD_ASIO_SRC}/handlers/handlers_recallmon.cpp
    ${TWORLD_ASIO_SRC}/handlers/handlers_conn.cpp
    ${TWORLD_ASIO_SRC}/handlers/handlers_relay.cpp
    ${TWORLD_ASIO_SRC}/senders/senders_relay.cpp
    ${TWORLD_ASIO_SRC}/senders/senders_guild.cpp
    ${TWORLD_ASIO_SRC}/senders/senders_party.cpp
    ${TWORLD_ASIO_SRC}/senders/senders_corps.cpp
    ${TWORLD_ASIO_SRC}/senders/senders_friend.cpp
    ${TWORLD_ASIO_SRC}/senders/senders_chat.cpp
    ${TWORLD_ASIO_SRC}/senders/senders_soulmate.cpp
    ${TWORLD_ASIO_SRC}/senders/senders_tms.cpp
    ${TWORLD_ASIO_SRC}/senders/senders_post.cpp
    ${TWORLD_ASIO_SRC}/senders/senders_occupy.cpp
    ${TWORLD_ASIO_SRC}/senders/senders_event.cpp
    ${TWORLD_ASIO_SRC}/senders/senders_combat.cpp
    ${TWORLD_ASIO_SRC}/senders/senders_item.cpp
    ${TWORLD_ASIO_SRC}/senders/senders_admin.cpp
    ${TWORLD_ASIO_SRC}/senders/senders_rank.cpp
    ${TWORLD_ASIO_SRC}/senders/senders_recallmon.cpp
    ${TWORLD_ASIO_SRC}/senders/senders_conn.cpp
    ${TWORLD_ASIO_SRC}/services/char_registry.cpp
    ${TWORLD_ASIO_SRC}/services/guild_broadcast.cpp
    ${TWORLD_ASIO_SRC}/services/guild_level_cache.cpp
    ${TWORLD_ASIO_SRC}/services/guild_peerage.cpp
    ${TWORLD_ASIO_SRC}/services/guild_registry.cpp
    ${TWORLD_ASIO_SRC}/services/party_registry.cpp
    ${TWORLD_ASIO_SRC}/services/corps_registry.cpp
    ${TWORLD_ASIO_SRC}/services/tms_registry.cpp
    ${TWORLD_ASIO_SRC}/services/fake_friend_repository.cpp
    ${TWORLD_ASIO_SRC}/services/soci_friend_repository.cpp
    ${TWORLD_ASIO_SRC}/services/guild_wanted_registry.cpp
    ${TWORLD_ASIO_SRC}/services/guild_wanted_sweep.cpp
    ${TWORLD_ASIO_SRC}/services/guild_tactics_wanted_registry.cpp
    ${TWORLD_ASIO_SRC}/services/guild_tactics_sweep.cpp
    ${TWORLD_ASIO_SRC}/services/guild_cabinet_codec.cpp
    ${TWORLD_ASIO_SRC}/services/pvp_aggregate.cpp
    ${TWORLD_ASIO_SRC}/services/fake_guild_level_repository.cpp
    ${TWORLD_ASIO_SRC}/services/fake_guild_repository.cpp
    ${TWORLD_ASIO_SRC}/services/soci_guild_level_repository.cpp
    ${TWORLD_ASIO_SRC}/services/soci_guild_repository.cpp
    ${TWORLD_ASIO_SRC}/services/peer_registry.cpp)

target_include_directories(tworldsvr_asio_core PUBLIC
    ${TWORLD_ASIO_SRC})

target_link_libraries(tworldsvr_asio_core PUBLIC
    fourstory_common
    tnetlib_portable
    ${TWORLD_SOCI_LIBS})

if(spdlog_FOUND)
    target_link_libraries(tworldsvr_asio_core PUBLIC spdlog::spdlog)
else()
    target_link_libraries(tworldsvr_asio_core PUBLIC spdlog)
endif()
if(tomlplusplus_FOUND)
    target_link_libraries(tworldsvr_asio_core PUBLIC tomlplusplus::tomlplusplus)
endif()

set_target_properties(tworldsvr_asio_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON)

add_executable(tworldsvr_asio ${TWORLD_ASIO_SRC}/main.cpp)
target_link_libraries(tworldsvr_asio PRIVATE tworldsvr_asio_core)

# ----- Tests --------------------------------------------------------
#
# W1 wire test: stands the WorldServer up on an ephemeral port,
# opens a loopback socket, frames a packet with a known + an unknown
# wID, and verifies (a) both are dispatched without closing the
# connection, (b) a checksum-mismatch frame DOES close the connection.
add_executable(test_tworldsvr_asio_dispatch
    ${TWORLD_ASIO_SRC}/tests/test_dispatch.cpp)
target_link_libraries(test_tworldsvr_asio_dispatch PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_dispatch
    COMMAND test_tworldsvr_asio_dispatch)

# W2 unit test: CharRegistry partitioning + per-char actor model.
# Exercises Insert / Find / Remove + concurrent inserts across
# threads + the active-user index. Pure logic — no asio, no DB.
add_executable(test_tworldsvr_asio_char_registry
    ${TWORLD_ASIO_SRC}/tests/test_char_registry.cpp)
target_link_libraries(test_tworldsvr_asio_char_registry PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_char_registry
    COMMAND test_tworldsvr_asio_char_registry)

# W2 wire test: drives MW_ADDCHAR_ACK + MW_CLOSECHAR_ACK frames
# through the dispatcher over a loopback session, asserts the
# CharRegistry state after each. Verifies the binding from
# wire → handler → registry mutation is correct.
add_executable(test_tworldsvr_asio_char_handlers
    ${TWORLD_ASIO_SRC}/tests/test_char_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_char_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_char_handlers
    COMMAND test_tworldsvr_asio_char_handlers)

# W3a-1 unit test: GuildRegistry partition / lifecycle. Pure
# logic, no asio. Mirrors test_char_registry's coverage.
add_executable(test_tworldsvr_asio_guild_registry
    ${TWORLD_ASIO_SRC}/tests/test_guild_registry.cpp)
target_link_libraries(test_tworldsvr_asio_guild_registry PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_guild_registry
    COMMAND test_tworldsvr_asio_guild_registry)

# W3a-1 wire test: drives DM_GUILDLOAD_ACK across a loopback
# session and checks the GuildRegistry insert + idempotent
# rejection of duplicate guild_id + char-not-in-registry drop.
add_executable(test_tworldsvr_asio_guild_handlers
    ${TWORLD_ASIO_SRC}/tests/test_guild_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_guild_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_guild_handlers
    COMMAND test_tworldsvr_asio_guild_handlers)

# W3a-1 fake-repo behavior: AddGuild seeds, LoadAll round-trips,
# FindById returns nullopt for misses. Pure logic.
add_executable(test_tworldsvr_asio_fake_guild_repo
    ${TWORLD_ASIO_SRC}/tests/test_fake_guild_repository.cpp)
target_link_libraries(test_tworldsvr_asio_fake_guild_repo PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_fake_guild_repo
    COMMAND test_tworldsvr_asio_fake_guild_repo)

# W3a-2 unit test: PeerRegistry lifecycle (Register / Find /
# Unregister, duplicate rejection, Snapshot). Pure logic.
add_executable(test_tworldsvr_asio_peer_registry
    ${TWORLD_ASIO_SRC}/tests/test_peer_registry.cpp)
target_link_libraries(test_tworldsvr_asio_peer_registry PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_peer_registry
    COMMAND test_tworldsvr_asio_peer_registry)

# W3a-2 wire test: drives RW_RELAYSVR_REQ over loopback, asserts
# (a) PeerRegistry gains an entry under the sent wID, (b) the
# RW_RELAYSVR_ACK reply carries the configured nation byte and the
# empty operator + msg lists. Verifies the full Wire → PeerSession
# → Registry path that W3a-3 handlers depend on.
add_executable(test_tworldsvr_asio_relay_handlers
    ${TWORLD_ASIO_SRC}/tests/test_relay_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_relay_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_relay_handlers
    COMMAND test_tworldsvr_asio_relay_handlers)

# W3a-3 unit test: CharRegistry secondary name index, including
# case-insensitive lookup, Rename, collision rejection, Remove
# dropping both indices, and concurrent renames on disjoint chars.
add_executable(test_tworldsvr_asio_char_name_index
    ${TWORLD_ASIO_SRC}/tests/test_char_name_index.cpp)
target_link_libraries(test_tworldsvr_asio_char_name_index PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_char_name_index
    COMMAND test_tworldsvr_asio_char_name_index)

# W3a-3 wire test: drives MW_CHANGECHARBASE_ACK over loopback,
# asserts the per-bType mutation lands on TChar (face/hair/race/sex/
# country/aid_country/name) + the name-index update on IK_NAME +
# the collision-refused branch.
add_executable(test_tworldsvr_asio_char_base_handlers
    ${TWORLD_ASIO_SRC}/tests/test_char_base_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_char_base_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_char_base_handlers
    COMMAND test_tworldsvr_asio_char_base_handlers)

# W3a-3 wire test: OnRW_ENTERCHAR_REQ (hit + miss) and
# OnRW_RELAYCONNECT_REQ routing to the right map peer by
# main_server_id, plus the cluster-wide RELAYCONNECT broadcast
# triggered by a second RELAYSVR registration.
add_executable(test_tworldsvr_asio_rw_lookup_handlers
    ${TWORLD_ASIO_SRC}/tests/test_rw_lookup_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_rw_lookup_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_rw_lookup_handlers
    COMMAND test_tworldsvr_asio_rw_lookup_handlers)

# W3a-4 wire test: MW_GUILDLEAVE_ACK + TChar.guild_id back-pointer
# invariant + the side-effect on a subsequent OnEnterCharReq.
add_executable(test_tworldsvr_asio_guild_mut_handlers
    ${TWORLD_ASIO_SRC}/tests/test_guild_mut_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_guild_mut_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_guild_mut_handlers
    COMMAND test_tworldsvr_asio_guild_mut_handlers)

# W3a-4d unit test: GuildLevelCache + FakeGuildLevelRepository
# round-trip + out-of-range row filtering + LoadFrom replace.
add_executable(test_tworldsvr_asio_guild_level_cache
    ${TWORLD_ASIO_SRC}/tests/test_guild_level_cache.cpp)
target_link_libraries(test_tworldsvr_asio_guild_level_cache PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_guild_level_cache
    COMMAND test_tworldsvr_asio_guild_level_cache)

# W3a-5 unit test: CheckPeerage gate. 7 scenarios cover every
# branch of the legacy switch + the W3a-4d "null level_row =
# relaxed dev gate" path.
add_executable(test_tworldsvr_asio_guild_peerage
    ${TWORLD_ASIO_SRC}/tests/test_guild_peerage.cpp)
target_link_libraries(test_tworldsvr_asio_guild_peerage PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_guild_peerage
    COMMAND test_tworldsvr_asio_guild_peerage)

# W3a-19 unit test: GuildWantedRegistry::PruneExpired +
# SweepExpiredWanted coroutine. Verifies the periodic prune
# only drops entries past end_time, cleans the applicant
# reverse index, and persists the deletions via the repo.
add_executable(test_tworldsvr_asio_guild_wanted_sweep
    ${TWORLD_ASIO_SRC}/tests/test_guild_wanted_sweep.cpp)
target_link_libraries(test_tworldsvr_asio_guild_wanted_sweep PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_guild_wanted_sweep
    COMMAND test_tworldsvr_asio_guild_wanted_sweep)

# W3a-28 unit test: CalcWeekRecord per-day vRecord trim + sum.
# Pure-logic, 6 scenarios covering empty / fresh / mixed / stale /
# boundary / idempotency.
add_executable(test_tworldsvr_asio_pvp_aggregate
    ${TWORLD_ASIO_SRC}/tests/test_pvp_aggregate.cpp)
target_link_libraries(test_tworldsvr_asio_pvp_aggregate PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_pvp_aggregate
    COMMAND test_tworldsvr_asio_pvp_aggregate)

# W3a-36 unit test: SweepExpiredTactics — prunes expired tactics
# contracts across the guild registry + clears char back-pointers.
add_executable(test_tworldsvr_asio_tactics_sweep
    ${TWORLD_ASIO_SRC}/tests/test_tactics_sweep.cpp)
target_link_libraries(test_tworldsvr_asio_tactics_sweep PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_tactics_sweep
    COMMAND test_tworldsvr_asio_tactics_sweep)

# W3b-1 unit test: PartyRegistry partition / lifecycle + TParty
# member-set helpers (IsChief / IsMember / Size / AddMember /
# RemoveMember). Pure logic, no asio. Mirrors test_guild_registry.
add_executable(test_tworldsvr_asio_party_registry
    ${TWORLD_ASIO_SRC}/tests/test_party_registry.cpp)
target_link_libraries(test_tworldsvr_asio_party_registry PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_party_registry
    COMMAND test_tworldsvr_asio_party_registry)

# W3b-1 wire test: drives MW_PARTYADD_ACK across a loopback session
# and asserts every validation branch of the invite gate
# (NOUSER / WAITERS / ALREADY / COUNTRY / NOTCHIEF / FULL) replies
# on the requester's socket, while the success branch relays
# PARTY_AGREE to the target's map peer + flags the target waiter.
add_executable(test_tworldsvr_asio_party_handlers
    ${TWORLD_ASIO_SRC}/tests/test_party_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_party_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_party_handlers
    COMMAND test_tworldsvr_asio_party_handlers)

# W3b-2 wire test: MW_PARTYJOIN_ACK formation over a three-peer
# loopback session — decline / inviter-offline relays + new-party
# creation (pairwise PARTYJOIN_REQ + PARTYATTR + registry state +
# back-pointers) + a third char joining the existing party.
add_executable(test_tworldsvr_asio_party_join_handlers
    ${TWORLD_ASIO_SRC}/tests/test_party_join_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_party_join_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_party_join_handlers
    COMMAND test_tworldsvr_asio_party_join_handlers)

# W3b-3 wire test: MW_PARTYDEL_ACK leave/kick over a four-peer
# loopback session — chief-leave succession, non-chief kick
# (flag + survival), and the disband cascade (2→0 with the last
# member pulled out + the party dropped from the registry).
add_executable(test_tworldsvr_asio_party_del_handlers
    ${TWORLD_ASIO_SRC}/tests/test_party_del_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_party_del_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_party_del_handlers
    COMMAND test_tworldsvr_asio_party_del_handlers)

# W3b-4 wire test: party attribute changes over a three-peer
# loopback session — MANSTAT member-stat broadcast + stored-stat
# update, CHGPARTYTYPE (non-chief reject / chief broadcast), and
# CHGPARTYCHIEF (success + roster PARTYATTR refresh / not-chief /
# already gates).
add_executable(test_tworldsvr_asio_party_attr_handlers
    ${TWORLD_ASIO_SRC}/tests/test_party_attr_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_party_attr_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_party_attr_handlers
    COMMAND test_tworldsvr_asio_party_attr_handlers)

# W3b-5 wire test: party member recall over a three-peer loopback
# session — summon (TP_RECALL) + move-to (TP_MOVETO) RECALLANS_REQ
# forwarding, the non-party-member busy reject, and the RECALLANS
# relay back to the teleported char (success + map-mismatch +
# small-meeting-room IU_TARGETBUSY gates).
add_executable(test_tworldsvr_asio_party_recall_handlers
    ${TWORLD_ASIO_SRC}/tests/test_party_recall_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_party_recall_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_party_recall_handlers
    COMMAND test_tworldsvr_asio_party_recall_handlers)

# W3b-6 wire test: PT_ORDER round-robin loot over a three-peer
# loopback session — turn-cursor selection across drops (item
# round-tripped through the W3a-37 cabinet codec) + the stale-party
# MIT_NOTFOUND reply.
add_executable(test_tworldsvr_asio_party_order_handlers
    ${TWORLD_ASIO_SRC}/tests/test_party_order_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_party_order_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_party_order_handlers
    COMMAND test_tworldsvr_asio_party_order_handlers)

# W3c-1 unit test: CorpsRegistry partition / lifecycle + TCorps
# squad-set helpers.
add_executable(test_tworldsvr_asio_corps_registry
    ${TWORLD_ASIO_SRC}/tests/test_corps_registry.cpp)
target_link_libraries(test_tworldsvr_asio_corps_registry PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_corps_registry
    COMMAND test_tworldsvr_asio_corps_registry)

# W3c-1 wire test: MW_CORPSASK_ACK invite-relay gate over a
# three-peer loopback session — success forward + WRONG_TARGET /
# NO_PARTY / BUSY / MAX_PARTY / both-in-corps reject branches.
add_executable(test_tworldsvr_asio_corps_handlers
    ${TWORLD_ASIO_SRC}/tests/test_corps_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_corps_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_corps_handlers
    COMMAND test_tworldsvr_asio_corps_handlers)

# W3c-2 wire test: MW_CORPSREPLY_ACK formation over a three-peer
# loopback session — decline relay, new-corps creation (pairwise
# ADDSQUAD + CORPSJOIN + commander PARTYATTR + registry state), and
# a third party joining the existing corps.
add_executable(test_tworldsvr_asio_corps_reply_handlers
    ${TWORLD_ASIO_SRC}/tests/test_corps_reply_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_corps_reply_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_corps_reply_handlers
    COMMAND test_tworldsvr_asio_corps_reply_handlers)

# W3c-3 wire test: MW_CORPSLEAVE_ACK leave/dissolve over a
# three-peer loopback session — unauthorized-kick reject, commander
# succession (3→2 squads), and the dissolve cascade (2→0 with the
# last squad pulled out + the corps dropped from the registry).
add_executable(test_tworldsvr_asio_corps_leave_handlers
    ${TWORLD_ASIO_SRC}/tests/test_corps_leave_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_corps_leave_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_corps_leave_handlers
    COMMAND test_tworldsvr_asio_corps_leave_handlers)

# W3c-4 wire test: MW_CHGCORPSCOMMANDER_ACK over a three-peer
# loopback session — non-commander / current-commander /
# non-member reject branches + the general's successful reassign
# (reply + every-squad commander refresh + registry update).
add_executable(test_tworldsvr_asio_corps_commander_handlers
    ${TWORLD_ASIO_SRC}/tests/test_corps_commander_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_corps_commander_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_corps_commander_handlers
    COMMAND test_tworldsvr_asio_corps_commander_handlers)

# W3c-5 wire test: MW_PARTYMOVE_ACK corps squad reshuffle — key /
# no-party / own-party / size-1-swap reject branches + a successful
# member move between squads (registry membership + chief succession).
add_executable(test_tworldsvr_asio_party_move_handlers
    ${TWORLD_ASIO_SRC}/tests/test_party_move_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_party_move_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_party_move_handlers
    COMMAND test_tworldsvr_asio_party_move_handlers)

# W3c-6 wire test: MW_CORPSCMD_ACK command broadcast over a
# three-peer loopback session — the general's order reaches every
# corps member; a corps-less issuer reaches only their own party.
add_executable(test_tworldsvr_asio_corps_cmd_handlers
    ${TWORLD_ASIO_SRC}/tests/test_corps_cmd_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_corps_cmd_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_corps_cmd_handlers
    COMMAND test_tworldsvr_asio_corps_cmd_handlers)

# W3c-7 wire test: the corps chief-to-chief relays (enemy-list
# family + CORPSHP) over a four-peer loopback session — the chief's
# payload reaches every other squad chief (tail intact, char/key
# swapped), non-chief senders are dropped, and the per-message wID
# mapping holds.
add_executable(test_tworldsvr_asio_corps_enemy_handlers
    ${TWORLD_ASIO_SRC}/tests/test_corps_enemy_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_corps_enemy_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_corps_enemy_handlers
    COMMAND test_tworldsvr_asio_corps_enemy_handlers)

# W4-1 wire test: MW_FRIENDASK_ACK invite gate over a three-peer
# loopback session — NOTFOUND / relay / ALREADY / mutual instant-add
# (both pending FT_TARGET upgrade + regions swapped) / country.
add_executable(test_tworldsvr_asio_friend_handlers
    ${TWORLD_ASIO_SRC}/tests/test_friend_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_friend_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_friend_handlers
    COMMAND test_tworldsvr_asio_friend_handlers)

# W4-2 wire test: MW_FRIENDREPLY_ACK (accept/reject/offline) +
# MW_FRIENDERASE_ACK (mutual demote / not-found / one-way remove)
# over a three-peer loopback session.
add_executable(test_tworldsvr_asio_friend_reply_handlers
    ${TWORLD_ASIO_SRC}/tests/test_friend_reply_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_friend_reply_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_friend_reply_handlers
    COMMAND test_tworldsvr_asio_friend_reply_handlers)

# W4-3 wire test: friend-group handlers (MAKE/DELETE/CHANGE/NAME)
# over a single-peer loopback session — all result branches +
# the friend-group membership / TFriend.group state transitions.
add_executable(test_tworldsvr_asio_friend_group_handlers
    ${TWORLD_ASIO_SRC}/tests/test_friend_group_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_friend_group_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_friend_group_handlers
    COMMAND test_tworldsvr_asio_friend_group_handlers)

# W4-4 wire test: MW_FRIENDLIST_ACK → MW_FRIENDLIST_REQ — groups +
# non-pending friends with online friends resolved live (level/
# class/connected/region) and offline ones zeroed; FT_TARGET
# excluded; soulmate sentinel.
add_executable(test_tworldsvr_asio_friend_list_handlers
    ${TWORLD_ASIO_SRC}/tests/test_friend_list_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_friend_list_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_friend_list_handlers
    COMMAND test_tworldsvr_asio_friend_list_handlers)

# W4-5 wire test: MW_CHAT_ACK channel relay over a three-peer
# loopback session — GUILD / PARTY / WORLD (per-peer) / WHISPER
# (recipient + sender echo) + the war-country whisper block.
add_executable(test_tworldsvr_asio_chat_handlers
    ${TWORLD_ASIO_SRC}/tests/test_chat_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_chat_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_chat_handlers
    COMMAND test_tworldsvr_asio_chat_handlers)

# W4-6 wire test: soulmate SEARCH/REG/END over a three-peer
# loopback session — matchmaking + tiebreaks + mutual pairing,
# REG preview vs register, cross-country FAIL, and END dissolve.
add_executable(test_tworldsvr_asio_soulmate_handlers
    ${TWORLD_ASIO_SRC}/tests/test_soulmate_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_soulmate_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_soulmate_handlers
    COMMAND test_tworldsvr_asio_soulmate_handlers)

# W4-7 wire test: social presence on logout — OnCloseCharAck fans
# FRIENDCONNECTION(DISCONNECTION) to a mutual friend + marks the
# reverse friend/soulmate entries offline; a one-way friend's entry
# is marked offline without a packet.
add_executable(test_tworldsvr_asio_friend_presence_handlers
    ${TWORLD_ASIO_SRC}/tests/test_friend_presence_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_friend_presence_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_friend_presence_handlers
    COMMAND test_tworldsvr_asio_friend_presence_handlers)

# W4-8 wire test: MW_REGION_ACK propagation — stores the char's
# region and mirrors it into the soulmate + mutual-friend reverse
# entries (and skips one-way FT_FRIEND); no packet emitted.
add_executable(test_tworldsvr_asio_region_handlers
    ${TWORLD_ASIO_SRC}/tests/test_region_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_region_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_region_handlers
    COMMAND test_tworldsvr_asio_region_handlers)

# W4-9 wire test: MW_LEVELUP_ACK — level store + soulmate sync /
# auto-dissolve on level gap + multi-connection LEVELUP_REQ fan-out.
add_executable(test_tworldsvr_asio_levelup_handlers
    ${TWORLD_ASIO_SRC}/tests/test_levelup_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_levelup_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_levelup_handlers
    COMMAND test_tworldsvr_asio_levelup_handlers)

# W4-10 wire test: inspect-player stat relay (CHARSTATINFO /
# CHARSTATINFOANS) — request routed to the target's map, the
# gathered stat block forwarded verbatim back to the requester.
add_executable(test_tworldsvr_asio_charstat_handlers
    ${TWORLD_ASIO_SRC}/tests/test_charstat_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_charstat_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_charstat_handlers
    COMMAND test_tworldsvr_asio_charstat_handlers)

# W4-11 wire test: TMS conference channels (INVITE opens a group,
# SEND fans the message, INVITEASK accepts a re-pair, OUT tears the
# group down) over a three-peer loopback session.
add_executable(test_tworldsvr_asio_tms_handlers
    ${TWORLD_ASIO_SRC}/tests/test_tms_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_tms_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_tms_handlers
    COMMAND test_tworldsvr_asio_tms_handlers)

# W4-12 wire test: TMS presence on logout (LeaveTMS) — a char in a
# conference logs out; the surviving member is told via TMSOUT_REQ.
add_executable(test_tworldsvr_asio_tms_logout_handlers
    ${TWORLD_ASIO_SRC}/tests/test_tms_logout_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_tms_logout_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_tms_logout_handlers
    COMMAND test_tworldsvr_asio_tms_logout_handlers)

# W4-13 wire test: mail delivery relay — player POSTRECV + DB-side
# RESERVEDPOSTRECV both forward MW_POSTRECV_REQ verbatim to the
# recipient's map (routed by target name).
add_executable(test_tworldsvr_asio_post_handlers
    ${TWORLD_ASIO_SRC}/tests/test_post_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_post_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_post_handlers
    COMMAND test_tworldsvr_asio_post_handlers)

# W4-14 wire test: per-character visual state sync — PETRIDING fans
# the mount to a char's other map session; HELMETHIDE confirms back
# to the originating map.
add_executable(test_tworldsvr_asio_appearance_handlers
    ${TWORLD_ASIO_SRC}/tests/test_appearance_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_appearance_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_appearance_handlers
    COMMAND test_tworldsvr_asio_appearance_handlers)

# W4-15 wire test: friend/group/soulmate load-at-login — a seeded
# FakeFriendRepository hydrates TChar.friends/friend_groups on
# MW_ADDCHAR_ACK with correct derived types + live connected flags.
add_executable(test_tworldsvr_asio_friend_load_handlers
    ${TWORLD_ASIO_SRC}/tests/test_friend_load_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_friend_load_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_friend_load_handlers
    COMMAND test_tworldsvr_asio_friend_load_handlers)

# W4-16 wire test: friend-group write-back — MAKE/NAME/CHANGE/DELETE
# persist through the (fake) repo in addition to the in-memory change.
add_executable(test_tworldsvr_asio_friend_persist_handlers
    ${TWORLD_ASIO_SRC}/tests/test_friend_persist_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_friend_persist_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_friend_persist_handlers
    COMMAND test_tworldsvr_asio_friend_persist_handlers)

# W4-17 wire test: friend-edge write-back — accept persists both
# directed edges; remove deletes only the requester's forward edge.
add_executable(test_tworldsvr_asio_friend_edge_handlers
    ${TWORLD_ASIO_SRC}/tests/test_friend_edge_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_friend_edge_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_friend_edge_handlers
    COMMAND test_tworldsvr_asio_friend_edge_handlers)

# W4-18 wire test: soulmate write-back — REG persists both mutual
# rows; END deletes them.
add_executable(test_tworldsvr_asio_soulmate_persist_handlers
    ${TWORLD_ASIO_SRC}/tests/test_soulmate_persist_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_soulmate_persist_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_soulmate_persist_handlers
    COMMAND test_tworldsvr_asio_soulmate_persist_handlers)

# W4-19 wire test: GM chat ban — ban/extend/unban an online target +
# enforce on the target's map + echo to the issuing GM.
add_executable(test_tworldsvr_asio_chatban_handlers
    ${TWORLD_ASIO_SRC}/tests/test_chatban_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_chatban_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_chatban_handlers
    COMMAND test_tworldsvr_asio_chatban_handlers)

# W4-20 wire test: login finalization — ENTERSVR indexes the name +
# stores identity/region + fans the friend connect-presence toast.
add_executable(test_tworldsvr_asio_entersvr_handlers
    ${TWORLD_ASIO_SRC}/tests/test_entersvr_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_entersvr_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_entersvr_handlers
    COMMAND test_tworldsvr_asio_entersvr_handlers)

# W5-1 wire test: territory occupation broadcasts — CASTLE/LOCAL/
# MISSION occupy fan the new owner+flag to every map peer (+ the
# LOCAL B-country display flip).
add_executable(test_tworldsvr_asio_occupy_handlers
    ${TWORLD_ASIO_SRC}/tests/test_occupy_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_occupy_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_occupy_handlers
    COMMAND test_tworldsvr_asio_occupy_handlers)

# W5-2 wire test: castle-war apply — chief assigns a member to a
# castle → SUCCESS to both maps + applicant-count broadcast.
add_executable(test_tworldsvr_asio_castle_apply_handlers
    ${TWORLD_ASIO_SRC}/tests/test_castle_apply_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_castle_apply_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_castle_apply_handlers
    COMMAND test_tworldsvr_asio_castle_apply_handlers)

# W5-3 wire test: CASTLEOCCUPY resets castle applications — the won
# castle's applicants are cleared + told, alongside the broadcast.
add_executable(test_tworldsvr_asio_castle_occupy_reset_handlers
    ${TWORLD_ASIO_SRC}/tests/test_castle_occupy_reset_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_castle_occupy_reset_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_castle_occupy_reset_handlers
    COMMAND test_tworldsvr_asio_castle_occupy_reset_handlers)

# W5-4 wire test: war-window enable broadcast — SM_BATTLESTATUS fans
# the LOCAL/CASTLE/MISSION enable packet to every map peer.
add_executable(test_tworldsvr_asio_battlestatus_handlers
    ${TWORLD_ASIO_SRC}/tests/test_battlestatus_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_battlestatus_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_battlestatus_handlers
    COMMAND test_tworldsvr_asio_battlestatus_handlers)

# W6-1 wire test: timed-event broadcast — EVENTQUARTER fans the
# present event to every peer; EVENTQUARTERNOTIFY broadcasts a
# world-chat announcement.
add_executable(test_tworldsvr_asio_event_handlers
    ${TWORLD_ASIO_SRC}/tests/test_event_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_event_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_event_handlers
    COMMAND test_tworldsvr_asio_event_handlers)

# W6-2 wire test: combat / taming cross-server relays — MAGICMIRROR /
# MONTEMPT / MONTEMPTEVO route to the attacker char's map.
add_executable(test_tworldsvr_asio_combat_handlers
    ${TWORLD_ASIO_SRC}/tests/test_combat_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_combat_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_combat_handlers
    COMMAND test_tworldsvr_asio_combat_handlers)

# W6-6 wire test: monster-result relays — MONSTERDIE / TAKEMONMONEY
# route verbatim to the char's main map.
add_executable(test_tworldsvr_asio_monresult_handlers
    ${TWORLD_ASIO_SRC}/tests/test_monresult_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_monresult_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_monresult_handlers
    COMMAND test_tworldsvr_asio_monresult_handlers)

# W6-7 wire test: solo-instance party lifecycle — ENTERSOLOMAP spins
# up a PT_SOLO party + mirrors it to the char's connections;
# LEAVESOLOMAP tears it down.
add_executable(test_tworldsvr_asio_solomap_handlers
    ${TWORLD_ASIO_SRC}/tests/test_solomap_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_solomap_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_solomap_handlers
    COMMAND test_tworldsvr_asio_solomap_handlers)

# W6-8 wire test: GM char message relay — CT_CHARMSG routes to the
# named char's main map.
add_executable(test_tworldsvr_asio_charmsg_handlers
    ${TWORLD_ASIO_SRC}/tests/test_charmsg_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_charmsg_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_charmsg_handlers
    COMMAND test_tworldsvr_asio_charmsg_handlers)

# W6-9 wire test: friend-protected refuse relay — a friend request to
# a protected target is relayed as a refuse to the target's map.
add_executable(test_tworldsvr_asio_friend_protected_handlers
    ${TWORLD_ASIO_SRC}/tests/test_friend_protected_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_friend_protected_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_friend_protected_handlers
    COMMAND test_tworldsvr_asio_friend_protected_handlers)

# W6-10 wire test: item-result relays — ADDITEMRESULT routes to the
# requesting map server; DEALITEMERROR routes to the target's map.
add_executable(test_tworldsvr_asio_item_handlers
    ${TWORLD_ASIO_SRC}/tests/test_item_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_item_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_item_handlers
    COMMAND test_tworldsvr_asio_item_handlers)

# W6-11 wire test: day-change guild ranking — SM_CHANGEDAY recomputes
# each guild's PvP total/month rank.
add_executable(test_tworldsvr_asio_guildrank_handlers
    ${TWORLD_ASIO_SRC}/tests/test_guildrank_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_guildrank_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_guildrank_handlers
    COMMAND test_tworldsvr_asio_guildrank_handlers)

# W6-12 wire test: GM user-tracking relays — USERPOSITION + USERMOVE
# route to the target char's map.
add_executable(test_tworldsvr_asio_admin_handlers
    ${TWORLD_ASIO_SRC}/tests/test_admin_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_admin_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_admin_handlers
    COMMAND test_tworldsvr_asio_admin_handlers)

# W6-3 wire test: global announcement broadcasts — FAMERANKUPDATE
# (verbatim) + HEROSELECT fan to every map peer.
add_executable(test_tworldsvr_asio_rank_handlers
    ${TWORLD_ASIO_SRC}/tests/test_rank_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_rank_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_rank_handlers
    COMMAND test_tworldsvr_asio_rank_handlers)

# W6-4 wire test: recall-mon sync — CREATE assigns the recall id +
# mirrors to the char's connections; DATA/DEL forward verbatim.
add_executable(test_tworldsvr_asio_recallmon_handlers
    ${TWORLD_ASIO_SRC}/tests/test_recallmon_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_recallmon_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_recallmon_handlers
    COMMAND test_tworldsvr_asio_recallmon_handlers)

# W6-5 wire test: companion-mon (spolecnik) sync — CREATE assigns the
# recall id + mirrors to the char's connections; DEL forwards verbatim.
add_executable(test_tworldsvr_asio_spolecnikmon_handlers
    ${TWORLD_ASIO_SRC}/tests/test_spolecnikmon_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_spolecnikmon_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_spolecnikmon_handlers
    COMMAND test_tworldsvr_asio_spolecnikmon_handlers)

# W6-13 wire test: connection-list reconcile — CONLIST/MAPSVRLIST
# drop no-longer-reported cons to dead_cons, ROUTELIST new servers
# via the main map, and CHECKMAIN every remaining connection.
add_executable(test_tworldsvr_asio_conn_handlers
    ${TWORLD_ASIO_SRC}/tests/test_conn_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_conn_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_conn_handlers
    COMMAND test_tworldsvr_asio_conn_handlers)

# W6-14 wire test: CHECKMAIN_ACK — main confirmation closes dead cons
# + CONRESULTs the live set; non-main handoff RELEASEMAINs the old main
# and re-points main; unknown char replies DELCHAR.
add_executable(test_tworldsvr_asio_checkmain_handlers
    ${TWORLD_ASIO_SRC}/tests/test_checkmain_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_checkmain_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_checkmain_handlers
    COMMAND test_tworldsvr_asio_checkmain_handlers)

# W6-15 wire test: RELEASEMAIN_ACK — old main releases → forward the
# char verbatim to the new main as ENTERSVR_REQ + record chg_main_id;
# new main offline → INVALIDCHAR; unknown char → DELCHAR.
add_executable(test_tworldsvr_asio_releasemain_handlers
    ${TWORLD_ASIO_SRC}/tests/test_releasemain_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_releasemain_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_releasemain_handlers
    COMMAND test_tworldsvr_asio_releasemain_handlers)

# W6-16 wire test: ENTERSVR_ACK handoff completion — chg_main_id set
# (normal map) → clear it + MAPSVRLIST to the new main; BR_SERVER_ID
# excluded (falls through to fresh login, flag intact).
add_executable(test_tworldsvr_asio_entersvr_handoff_handlers
    ${TWORLD_ASIO_SRC}/tests/test_entersvr_handoff_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_entersvr_handoff_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_entersvr_handoff_handlers
    COMMAND test_tworldsvr_asio_entersvr_handoff_handlers)

# W6-17 wire test: teleport begin + cession queue — same-channel fast
# path, single BEGINTELEPORT → STARTTELEPORT broadcast, and a deferred
# second teleport that replays when CHECKMAIN_ACK pops the first.
add_executable(test_tworldsvr_asio_teleport_handlers
    ${TWORLD_ASIO_SRC}/tests/test_teleport_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_teleport_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_teleport_handlers
    COMMAND test_tworldsvr_asio_teleport_handlers)

# W6-18 wire test: CHECKCONNECT_ACK — count=0 updates position +
# CHECKMAIN sweep; count>0 with a new server → ROUTELIST via main;
# unknown char → DELCHAR.
add_executable(test_tworldsvr_asio_checkconnect_handlers
    ${TWORLD_ASIO_SRC}/tests/test_checkconnect_handlers.cpp)
target_link_libraries(test_tworldsvr_asio_checkconnect_handlers PRIVATE
    tworldsvr_asio_core)
add_test(NAME tworldsvr_asio_checkconnect_handlers
    COMMAND test_tworldsvr_asio_checkconnect_handlers)
