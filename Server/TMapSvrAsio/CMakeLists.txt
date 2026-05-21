# TMapSvrAsio — modernized 4Story map (gameplay) server. Work in
# progress, port-in-progress from the legacy Win32 / MFC / ATL
# Server/TMapSvr/ implementation, modeled on the other modernized
# Asio servers (TLoginSvrAsio, TControlSvrAsio, TPatchSvrAsio,
# TLogSvrAsio): Boost.Asio coroutines, spdlog, toml++, SOCI/ODBC,
# AsioSession from Lib/Own/TNetLib, MessageId from Lib/Own/TProtocol.
#
# Layout
# ------
#   main.cpp          F1 scaffold entry point (no listener yet).
#   legacy_src/       Verbatim copy of Server/TMapSvr/, source-of-truth
#                     reference material for the file-by-file port.
#                     Not compiled by this target.
#
# Phase plan (see PR #23 design doc): F1 main + scaffold (this commit),
# F2 config + schema validator, F3 MapServer accept loop, F4 handshake,
# F5 World peer, F6 movement, F7 channels+AOI, F8 combat, F9 items,
# F10 NPCs, F11 skills, F12 quests, F13 monsters/AI, F14 party+chat,
# F15 companions, F16 BR/Bow run-time mode, F17 control+audit.

set(TMAPSVR_ASIO_SRC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(spdlog QUIET)
# Boost::system is located by find_package(Boost ... COMPONENTS system)
# in the root CMakeLists.txt — Asio itself is header-only, but the
# error_code machinery resolves through boost_system.

add_executable(tmapsvr_asio ${TMAPSVR_ASIO_SRC}/main.cpp)

target_include_directories(tmapsvr_asio PRIVATE ${TMAPSVR_ASIO_SRC})

if(TARGET Boost::system)
    target_link_libraries(tmapsvr_asio PRIVATE Boost::system)
endif()
if(spdlog_FOUND)
    target_link_libraries(tmapsvr_asio PRIVATE spdlog::spdlog)
else()
    # Fallback for header-only spdlog discovery via include path only.
    target_link_libraries(tmapsvr_asio PRIVATE spdlog)
endif()
