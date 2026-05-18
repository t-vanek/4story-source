# TPatchSvrAsio — modernized 4Story patch metadata server. Plain-wire
# 8B-header codec (no XOR / no RC4 — legacy TPatchSvr uses raw packets),
# SOCI queries against TGLOBAL.TVERSION / TPREVERSION, 9 CT_* handlers.

set(TPATCHSVR_ASIO_SRC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(spdlog QUIET)
find_package(tomlplusplus QUIET)
find_package(SOCI CONFIG QUIET)

set(TPATCHSVR_SOCI_LIBS "")
if(SOCI_FOUND)
    if(TARGET SOCI::soci_core)
        list(APPEND TPATCHSVR_SOCI_LIBS SOCI::soci_core)
    elseif(TARGET SOCI::soci_core_static)
        list(APPEND TPATCHSVR_SOCI_LIBS SOCI::soci_core_static)
    endif()
    if(TARGET SOCI::soci_odbc)
        list(APPEND TPATCHSVR_SOCI_LIBS SOCI::soci_odbc)
    elseif(TARGET SOCI::soci_odbc_static)
        list(APPEND TPATCHSVR_SOCI_LIBS SOCI::soci_odbc_static)
    endif()
else()
    list(APPEND TPATCHSVR_SOCI_LIBS soci_odbc soci_core dl)
endif()

add_library(tpatchsvr_asio_core STATIC
    ${TPATCHSVR_ASIO_SRC}/patch_server.cpp
    ${TPATCHSVR_ASIO_SRC}/handlers.cpp
    ${TPATCHSVR_ASIO_SRC}/config.cpp
    ${TPATCHSVR_ASIO_SRC}/patch_session.cpp
    ${TPATCHSVR_ASIO_SRC}/services/patch_repository.cpp)

# Shared infrastructure (DB pool, audit, admin shell, health endpoint,
# rate limiter, registry refresher) lives in fourstory_common. tnetlib
# is the wire-codec layer; included for MessageId / AsioSession.
target_include_directories(tpatchsvr_asio_core PUBLIC
    ${TPATCHSVR_ASIO_SRC})

target_link_libraries(tpatchsvr_asio_core PUBLIC
    fourstory_common
    tnetlib_portable
    ${TPATCHSVR_SOCI_LIBS})

if(spdlog_FOUND)
    target_link_libraries(tpatchsvr_asio_core PUBLIC spdlog::spdlog)
else()
    target_link_libraries(tpatchsvr_asio_core PUBLIC spdlog)
endif()
if(tomlplusplus_FOUND)
    target_link_libraries(tpatchsvr_asio_core PUBLIC tomlplusplus::tomlplusplus)
endif()
set_target_properties(tpatchsvr_asio_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(tpatchsvr_asio ${TPATCHSVR_ASIO_SRC}/main.cpp)
target_link_libraries(tpatchsvr_asio PRIVATE tpatchsvr_asio_core)
