# TLogSvrAsio — modernized audit log collector. UDP-only ingest of the
# legacy `_UDPPACKET` frame (same struct TLoginSvrAsio's UdpAuditLogger
# already emits), SOCI INSERT into TLOG_AUDIT.

set(TLOGSVR_ASIO_SRC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(spdlog QUIET)
find_package(tomlplusplus QUIET)
find_package(SOCI CONFIG QUIET)

set(TLOGSVR_SOCI_LIBS "")
if(SOCI_FOUND)
    if(TARGET SOCI::soci_core)
        list(APPEND TLOGSVR_SOCI_LIBS SOCI::soci_core)
    elseif(TARGET SOCI::soci_core_static)
        list(APPEND TLOGSVR_SOCI_LIBS SOCI::soci_core_static)
    endif()
    if(TARGET SOCI::soci_odbc)
        list(APPEND TLOGSVR_SOCI_LIBS SOCI::soci_odbc)
    elseif(TARGET SOCI::soci_odbc_static)
        list(APPEND TLOGSVR_SOCI_LIBS SOCI::soci_odbc_static)
    endif()
else()
    list(APPEND TLOGSVR_SOCI_LIBS soci_odbc soci_core dl)
endif()

add_library(tlogsvr_asio_core STATIC
    ${TLOGSVR_ASIO_SRC}/log_server.cpp
    ${TLOGSVR_ASIO_SRC}/config.cpp
    ${TLOGSVR_ASIO_SRC}/db/schema_validator.cpp
    ${TLOGSVR_ASIO_SRC}/services/log_sink.cpp)

target_include_directories(tlogsvr_asio_core PUBLIC
    ${TLOGSVR_ASIO_SRC})

target_link_libraries(tlogsvr_asio_core PUBLIC
    fourstory_common
    tnetlib_portable
    ${TLOGSVR_SOCI_LIBS})

if(spdlog_FOUND)
    target_link_libraries(tlogsvr_asio_core PUBLIC spdlog::spdlog)
else()
    target_link_libraries(tlogsvr_asio_core PUBLIC spdlog)
endif()
if(tomlplusplus_FOUND)
    target_link_libraries(tlogsvr_asio_core PUBLIC tomlplusplus::tomlplusplus)
endif()
set_target_properties(tlogsvr_asio_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(tlogsvr_asio ${TLOGSVR_ASIO_SRC}/main.cpp)
target_link_libraries(tlogsvr_asio PRIVATE tlogsvr_asio_core)

# ----- Tests --------------------------------------------------------
#
# Pure-unit: wire codec for the legacy _UDPPACKET / _LOG_DATA_ frame.
# Builds frames byte-for-byte the way UdpAuditLogger emits them and
# round-trips them through DecodeLogDatagram. No DB or network. Runs
# anywhere we can build the binary.
add_executable(test_tlogsvr_asio_log_decoder
    ${TLOGSVR_ASIO_SRC}/tests/test_log_decoder.cpp)
target_link_libraries(test_tlogsvr_asio_log_decoder PRIVATE
    tlogsvr_asio_core)
add_test(NAME tlogsvr_asio_log_decoder
    COMMAND test_tlogsvr_asio_log_decoder)

# Integration: ValidateAuditSchema + SociLogSink against a live MSSQL
# database. Skips when TLOGSVR_TEST_MSSQL_CONN is not in the
# environment, matching the convention the TLogin/TPatch SOCI tests
# use so CI shards without a DB still go green.
add_executable(test_tlogsvr_asio_soci_log_sink
    ${TLOGSVR_ASIO_SRC}/tests/test_soci_log_sink.cpp)
target_link_libraries(test_tlogsvr_asio_soci_log_sink PRIVATE
    tlogsvr_asio_core)
add_test(NAME tlogsvr_asio_soci_log_sink
    COMMAND test_tlogsvr_asio_soci_log_sink)
