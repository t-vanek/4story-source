#pragma once

// Transaction — RAII wrapper for a SOCI session transaction.
// Commits on explicit Commit(), rolls back on destruction if not committed.
//
// Usage:
//   Transaction tx(sql);
//   repo.Insert(a);
//   repo.Insert(b);
//   tx.Commit();  // both land atomically

#include <soci/soci.h>
#include <spdlog/spdlog.h>

namespace fourstory::db::orm {

class Transaction
{
public:
    explicit Transaction(soci::session& sql)
        : m_sql(sql)
        , m_committed(false)
    {
        m_sql.begin();
    }

    void Commit()
    {
        m_sql.commit();
        m_committed = true;
    }

    void Rollback()
    {
        if (!m_committed)
        {
            try { m_sql.rollback(); } catch (...) {}
            m_committed = true;
        }
    }

    ~Transaction()
    {
        if (!m_committed)
        {
            try
            {
                m_sql.rollback();
            }
            catch (const std::exception& ex)
            {
                spdlog::warn("Transaction: rollback in destructor failed: {}",
                    ex.what());
            }
        }
    }

    // Non-copyable, movable
    Transaction(const Transaction&)            = delete;
    Transaction& operator=(const Transaction&) = delete;
    Transaction(Transaction&& o) noexcept
        : m_sql(o.m_sql), m_committed(o.m_committed)
    {
        o.m_committed = true; // prevent double-rollback
    }

private:
    soci::session& m_sql;
    bool           m_committed;
};

} // namespace fourstory::db::orm
