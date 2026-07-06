#include "services/soci_service_ops_repository.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <ctime>
#include <exception>

namespace tworldsvr {

namespace {

// Legacy __TIMETODB converts through CTime, i.e. local time.
std::tm ToLocalTm(std::int64_t unix_secs)
{
    std::time_t t = static_cast<std::time_t>(unix_secs);
    std::tm out{};
#ifdef _WIN32
    localtime_s(&out, &t);
#else
    localtime_r(&t, &out);
#endif
    return out;
}

} // namespace

bool SociServiceOpsRepository::SaveHelpMessage(std::uint8_t id,
                                               std::int64_t start_unix,
                                               std::int64_t end_unix,
                                               const std::string& message)
{
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        const int id_int = static_cast<int>(id);
        std::tm start_tm = ToLocalTm(start_unix);
        std::tm end_tm   = ToLocalTm(end_unix);
        sql << "EXEC dbo.THelpMessage :id, :s, :e, :m",
            soci::use(id_int), soci::use(start_tm), soci::use(end_tm),
            soci::use(message);
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociServiceOpsRepository::SaveHelpMessage({}) "
                      "failed: {}", id, ex.what());
        return false;
    }
}

bool SociServiceOpsRepository::ClearMapCurrentUser(std::uint8_t group_id,
                                                   std::uint8_t server_id,
                                                   std::uint8_t svr_type)
{
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        const int g = static_cast<int>(group_id);
        const int s = static_cast<int>(server_id);
        const int t = static_cast<int>(svr_type);
        sql << "EXEC dbo.TClearMapCurrentUser :g, :s, :t",
            soci::use(g), soci::use(s), soci::use(t);
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociServiceOpsRepository::ClearMapCurrentUser"
                      "({},{},{}) failed: {}", group_id, server_id,
                      svr_type, ex.what());
        return false;
    }
}

} // namespace tworldsvr
