# TMapSvrAsio — modernized 4Story map (gameplay) server. Work in
# progress, port-in-progress from the legacy Win32 / MFC / ATL
# Server/TMapSvr/ implementation, modeled on the other modernized
# Asio servers (TLoginSvrAsio, TControlSvrAsio, TPatchSvrAsio,
# TLogSvrAsio): Boost.Asio coroutines, spdlog, toml++, SOCI/ODBC,
# AsioSession from Lib/Own/TNetLib, MessageId from Lib/Own/TProtocol.
#
# Layout
# ------
#   main.cpp          F2 scaffold entry point (config + schema check).
#   config.h/cpp      TOML loader — port, mode, [database], [world].
#   tmapsvr.example.toml  Documented defaults.
#   db/               Per-server schema validators (boot-time fail-fast).
#   legacy_src/       Verbatim copy of Server/TMapSvr/, reference only,
#                     not compiled.
#
# Phase plan (see PR #23 design doc): F1 main scaffold (done), F2
# config + schema validator (this commit), F3 MapServer accept loop,
# F4 handshake, F5 World peer, F6 movement, F7 channels+AOI,
# F8 combat, F9 items, F10 NPCs, F11 skills, F12 quests,
# F13 monsters/AI, F14 party+chat, F15 companions, F16 BR/Bow run-time
# mode, F17 control+audit.

set(TMAPSVR_ASIO_SRC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(spdlog QUIET)
find_package(tomlplusplus QUIET)
find_package(SOCI CONFIG QUIET)

set(TMAPSVR_SOCI_LIBS "")
if(SOCI_FOUND)
    if(TARGET SOCI::soci_core)
        list(APPEND TMAPSVR_SOCI_LIBS SOCI::soci_core)
    elseif(TARGET SOCI::soci_core_static)
        list(APPEND TMAPSVR_SOCI_LIBS SOCI::soci_core_static)
    endif()
    if(TARGET SOCI::soci_odbc)
        list(APPEND TMAPSVR_SOCI_LIBS SOCI::soci_odbc)
    elseif(TARGET SOCI::soci_odbc_static)
        list(APPEND TMAPSVR_SOCI_LIBS SOCI::soci_odbc_static)
    endif()
else()
    list(APPEND TMAPSVR_SOCI_LIBS soci_odbc soci_core dl)
endif()

# Core static library — config, schema validator, and (from F3 on)
# the MapServer / handlers / services layer. Split from the executable
# so unit tests can link against the core without dragging in main().
add_library(tmapsvr_asio_core STATIC
    ${TMAPSVR_ASIO_SRC}/config.cpp
    ${TMAPSVR_ASIO_SRC}/db/schema_validator.cpp)

target_include_directories(tmapsvr_asio_core PUBLIC
    ${TMAPSVR_ASIO_SRC})

target_link_libraries(tmapsvr_asio_core PUBLIC
    fourstory_common
    ${TMAPSVR_SOCI_LIBS})

if(TARGET Boost::system)
    target_link_libraries(tmapsvr_asio_core PUBLIC Boost::system)
endif()
if(spdlog_FOUND)
    target_link_libraries(tmapsvr_asio_core PUBLIC spdlog::spdlog)
else()
    target_link_libraries(tmapsvr_asio_core PUBLIC spdlog)
endif()
if(tomlplusplus_FOUND)
    target_link_libraries(tmapsvr_asio_core PUBLIC tomlplusplus::tomlplusplus)
endif()

set_target_properties(tmapsvr_asio_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Executable entry point. Just main.cpp + link against the core; the
# rest of the wiring lives inside the static library.
add_executable(tmapsvr_asio ${TMAPSVR_ASIO_SRC}/main.cpp)
target_link_libraries(tmapsvr_asio PRIVATE tmapsvr_asio_core)
