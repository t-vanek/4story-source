#include "native_control.h"
#include <cstdlib>
#include <ctime>

namespace tcontrolsvr::native_control {
Owner::Owner(fourstory::db::SessionPool& pool):m_lease(pool.Acquire()) {
    if(!Enabled(pool)||pool.PoolSize()<2)throw std::runtime_error("Native Control needs PostgreSQL pool_size >= 2");
    auto& sql=*m_lease;int acquired=0;
    sql << "SELECT pg_try_advisory_lock(1413698130)::integer",soci::into(acquired);
    if(!acquired)throw std::runtime_error("Another Control instance owns this database");
    try {
        std::string timezone;
        sql << "SELECT legacy_timezone FROM app_control.runtime_contract WHERE contract=1",soci::into(timezone);
        if(!sql.got_data())throw std::runtime_error("Control metadata has not been imported");
        const auto* configured=std::getenv("TZ");
        if(!configured||timezone!=configured)
            throw std::runtime_error("TZ must match imported Control legacy_timezone");
        // Event scheduler and overlap checks use the same local-time semantics
        // as CTime. The operator selects TZ explicitly; no source zone is guessed.
#ifdef _WIN32
        _tzset();
#else
        tzset();
#endif
        soci::transaction tx(sql);
        sql << "INSERT INTO app_control.writer(singleton,generation,backend_pid,backend_start) "
               "SELECT true,1,pg_backend_pid(),backend_start FROM pg_stat_activity WHERE pid=pg_backend_pid() "
               "ON CONFLICT(singleton) DO UPDATE SET generation=app_control.writer.generation+1,"
               "backend_pid=EXCLUDED.backend_pid,backend_start=EXCLUDED.backend_start RETURNING generation,backend_pid",
            soci::into(m_generation),soci::into(m_pid);
        if(!sql.got_data())throw std::runtime_error("Control ownership unavailable");
        tx.commit();
    } catch(...) {
        try {sql << "SELECT pg_advisory_unlock(1413698130)";}catch(...){}
        throw;
    }
}
Owner::~Owner() {
    try {(*m_lease) << "SELECT pg_advisory_unlock(1413698130)";}catch(...){}
}
void Owner::Check(soci::session& sql) const {
    if(m_uncertain)throw std::runtime_error("Control commit outcome unknown; recovery requires restart");
    long long generation=0;
    sql << "SELECT w.generation FROM app_control.writer w WHERE w.singleton AND w.generation=:g AND w.backend_pid=:pid "
           "AND EXISTS(SELECT 1 FROM pg_stat_activity a JOIN pg_locks l ON l.pid=a.pid "
           "WHERE a.pid=w.backend_pid AND a.backend_start=w.backend_start AND l.locktype='advisory' "
           "AND l.classid=0 AND l.objid=1413698130 AND l.objsubid=1 AND l.granted AND l.mode='ExclusiveLock') FOR SHARE OF w",
        soci::use(m_generation,"g"),soci::use(m_pid,"pid"),soci::into(generation);
    if(!sql.got_data()||generation!=m_generation)throw std::runtime_error("Control writer ownership expired");
}
std::unique_ptr<soci::transaction> Owner::Begin(soci::session& sql) const {
    auto tx=std::make_unique<soci::transaction>(sql);Check(sql);return tx;
}
} // namespace tcontrolsvr::native_control
