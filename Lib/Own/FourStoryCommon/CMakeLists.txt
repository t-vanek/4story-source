# fourstory_common — shared infrastructure for the modernized servers
# (TLoginSvrAsio, TPatchSvrAsio, TLogSvrAsio). Pulled out so each server
# doesn't carry its own copy of the SOCI session pool, audit/SMTP
# plumbing, admin shell, health endpoint, rate limiter, and registry
# refresher.
#
# Public include layout: consumers write `#include "fourstory/db/...h"`,
# the directory tree under fourstory/ mirrors the namespace tree.

set(FOURSTORY_COMMON_HEADERS
    fourstory/db/session_pool.h
    fourstory/db/schema_validator.h
    fourstory/audit/audit_logger.h
    fourstory/audit/spdlog_audit_logger.h
    fourstory/audit/udp_audit_logger.h
    fourstory/smtp/smtp_client.h
    fourstory/smtp/spdlog_smtp_client.h
    fourstory/ops/admin_shell.h
    fourstory/ops/health_endpoint.h
    fourstory/ops/rate_limiter.h
    fourstory/ops/registry_refresher.h
)

set(FOURSTORY_COMMON_SOURCES
    src/db/session_pool.cpp
    src/db/schema_validator.cpp
    src/audit/spdlog_audit_logger.cpp
    src/audit/udp_audit_logger.cpp
    src/smtp/spdlog_smtp_client.cpp
    src/ops/admin_shell.cpp
    src/ops/health_endpoint.cpp
    src/ops/rate_limiter.cpp
    src/ops/registry_refresher.cpp
)

add_library(fourstory_common STATIC
    ${FOURSTORY_COMMON_HEADERS}
    ${FOURSTORY_COMMON_SOURCES}
)

target_include_directories(fourstory_common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Boost.Asio (headers only — system supplies error codes), spdlog, SOCI.
find_package(Boost CONFIG REQUIRED COMPONENTS system)
find_package(spdlog CONFIG REQUIRED)
find_package(SOCI CONFIG REQUIRED)

target_link_libraries(fourstory_common
    PUBLIC
        Boost::system
        spdlog::spdlog
        SOCI::soci_core
)

# ODBC backend is used at runtime via SOCI's dynamic backend loader;
# we keep the include path consistent with the SOCI ODBC backend so the
# session_pool.cpp `#include <soci/odbc/soci-odbc.h>` resolves.
if(TARGET SOCI::soci_odbc)
    target_link_libraries(fourstory_common PUBLIC SOCI::soci_odbc)
endif()
