#include "soci_user_protected_service.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

namespace tcontrolsvr {

SociUserProtectedService::SociUserProtectedService(
    fourstory::db::SessionPool& pool)
    : m_pool(pool)
{
}

std::uint8_t
SociUserProtectedService::AddBan(const std::string& user_id,
                                 std::uint32_t duration_days,
                                 const std::string& reason,
                                 std::uint8_t permanent,
                                 const std::string& operator_id)
{
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;

        int nret = 0;
        int duration = static_cast<int>(duration_days);
        int perm     = static_cast<int>(permanent);
        soci::statement st = (sql.prepare <<
            "{ ? = CALL TUserProtectedAdd(?, ?, ?, ?, ?) }",
            soci::into(nret),
            soci::use(user_id),
            soci::use(duration),
            soci::use(reason),
            soci::use(perm),
            soci::use(operator_id));
        st.execute(true);
        if (nret < 0 || nret > 255)
            return 0;
        return static_cast<std::uint8_t>(nret);
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_user_protected: TUserProtectedAdd('{}') failed: {}",
            user_id, ex.what());
        return 0;
    }
}

} // namespace tcontrolsvr
