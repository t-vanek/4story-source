#include "soci_user_protected_service.h"

#include "fourstory/db/orm/sp_call.h"

#include <spdlog/spdlog.h>
#include <bit>

namespace tcontrolsvr {

SociUserProtectedService::SociUserProtectedService(
    fourstory::db::SessionPool& pool, std::shared_ptr<native_control::Owner> owner)
    : m_owner(std::move(owner)), m_pool(pool)
{
}

std::uint8_t
SociUserProtectedService::AddBan(const std::string& user_id,
                                 std::uint32_t duration_days,
                                 const std::string& reason,
                                 std::uint8_t permanent,
                                 const std::string& operator_id)
{
    using fourstory::db::orm::SpCall;
    try
    {
        if(native_control::Enabled(m_pool)) {
            if(m_commit_uncertain)throw std::runtime_error("Previous Control ban commit has unknown outcome; restart required");
            if(user_id.size()>20||operator_id.size()>20||reason.size()>51||permanent>1||
                user_id.find('\0')!=std::string::npos||operator_id.find('\0')!=std::string::npos||reason.find('\0')!=std::string::npos)
                return 0;
            auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=native_control::Begin(sql,m_owner);
            const auto name=native_control::Hex(user_id),message=native_control::Hex(reason),op=native_control::Hex(operator_id);
            int user=0,character=0;
            // Backup TUserProtectedAdd targets TGAME_GSP and hardcodes bWorld=1.
            // Join the actual native account/character rather than the legacy
            // archival tables, which are not mutable authentication state.
            soci::rowset<soci::row> candidates=(sql.prepare << "SELECT c.\"dwUserID\",c.\"dwCharID\" FROM app_world.\"TCHARTABLE\" c "
                   "JOIN app_global.\"TACCOUNT_PW\" a ON a.\"dwUserID\"=c.\"dwUserID\" "
                   "WHERE c.\"bWorldID\"=1 AND rtrim(c.\"szNAME\") COLLATE app_global.account_name_ci="
                   "rtrim(convert_from(decode(:name,'hex'),'WIN1252')) COLLATE app_global.account_name_ci ORDER BY c.\"dwUserID\",c.\"dwCharID\" FOR UPDATE OF a,c",
                soci::use(name));
            bool found=false;
            for(const auto& candidate:candidates) {
                if(found)throw std::runtime_error("Ambiguous character name for account ban");
                user=candidate.get<int>(0);character=candidate.get<int>(1);found=true;
            }
            if(!found){tx->commit();return 0;}
            const int days=std::bit_cast<std::int32_t>(duration_days),eternal=permanent;
            sql << "INSERT INTO app_global.\"TUSERPROTECTED\"(\"dwUserID\",\"bBlockType\",\"bEternal\",\"bWorld\",\"dwCharID\",\"szCharName\","
                   "\"startTime\",\"dwDuration\",\"bBlockReason\",\"szComment\",\"szGMID\",\"regDate\",\"sentBanMail\") "
                   "VALUES(:u,1,:e,1,:c,convert_from(decode(:name,'hex'),'WIN1252'),clock_timestamp(),:days,1,"
                   "convert_from(decode(:reason,'hex'),'WIN1252'),convert_from(decode(:op,'hex'),'WIN1252'),clock_timestamp(),0)",
                soci::use(user,"u"),soci::use(eternal,"e"),soci::use(character,"c"),soci::use(name,"name"),
                soci::use(days,"days"),soci::use(message,"reason"),soci::use(op,"op");
            native_control::Commit(*tx,m_commit_uncertain,m_owner);
            return 1;
        }
        auto lease = m_pool.Acquire();
        auto r = SpCall("TUserProtectedAdd")
            .In("szUserID",    user_id)
            .In("dwDuration",  static_cast<int>(duration_days))
            .In("szReason",    reason)
            .In("bPermanent",  static_cast<int>(permanent))
            .In("szOperator",  operator_id)
            .WithReturn()
            .Execute(*lease);
        if (!r.Ok()) return 0;
        const auto ret = r.ReturnCode();
        if (ret < 0 || ret > 255) return 0;
        return static_cast<std::uint8_t>(ret);
    }
    catch (const std::exception& ex)
    {
        if(native_control::Enabled(m_pool))throw std::runtime_error("Native Control ban persistence unavailable");
        spdlog::error("soci_user_protected: TUserProtectedAdd('{}') failed: {}",
            user_id, ex.what());
        return 0;
    }
}

} // namespace tcontrolsvr
