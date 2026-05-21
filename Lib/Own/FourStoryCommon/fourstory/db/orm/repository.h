#pragma once

// Repository<T> — generic CRUD on top of EntityMapping<T> + SOCI.
//
// Requires a specialized EntityMapping<T> (see entity_mapping.h).
// Methods are synchronous (call from a worker thread or wrap with
// fourstory::db::CoOffloadIf if on the io_context).
//
// Example:
//   auto lease = pool.Acquire();
//   Repository<Account> repo(*lease);
//
//   auto acc  = repo.FindById(1234);        // std::optional<Account>
//   auto list = repo.Where("bLocked = 1");  // std::vector<Account>
//   repo.Insert(new_acc);
//   repo.Update(existing_acc);
//   repo.Delete(1234);
//   std::size_t n = repo.Count();

#include "entity_mapping.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace fourstory::db::orm {

template<typename T>
class Repository
{
    using Map = EntityMapping<T>;

public:
    explicit Repository(soci::session& sql) : m_sql(sql) {}

    // ── Queries ──────────────────────────────────────────────────────

    // Find by primary key.
    template<typename PkType>
    std::optional<T> FindById(PkType pk)
    {
        try
        {
            soci::rowset<soci::row> rs =
                (m_sql.prepare << Map::SelectByIdSql(),
                 soci::use(pk, "pk"));
            for (const auto& row : rs)
                return Map::FromRow(row);
        }
        catch (const std::exception& ex)
        {
            spdlog::error("Repository<{}>::FindById: {}",
                Map::Table, ex.what());
        }
        return std::nullopt;
    }

    // All rows, optionally filtered.
    // `where` is appended as-is after "WHERE " — use named SOCI params
    // and pass them in `bind`. For complex queries, prefer SpCall or raw SQL.
    std::vector<T> All()
    {
        return Query(Map::SelectAllSql());
    }

    std::vector<T> Where(const std::string& where_clause)
    {
        return Query(Map::SelectAllSql() + " WHERE " + where_clause);
    }

    std::size_t Count()
    {
        std::size_t n = 0;
        try
        {
            m_sql << "SELECT COUNT(*) FROM " + std::string(Map::Table),
                soci::into(n);
        }
        catch (const std::exception& ex)
        {
            spdlog::error("Repository<{}>::Count: {}", Map::Table, ex.what());
        }
        return n;
    }

    // ── Mutations ─────────────────────────────────────────────────────

    void Insert(const T& entity)
    {
        try
        {
            soci::statement st(m_sql);
            st.alloc();
            st.prepare(Map::InsertSql());
            Map::BindInsert(st, entity);
            st.define_and_bind();
            st.execute(true);
        }
        catch (const std::exception& ex)
        {
            spdlog::error("Repository<{}>::Insert: {}", Map::Table, ex.what());
            throw;
        }
    }

    void Update(const T& entity)
    {
        try
        {
            soci::statement st(m_sql);
            st.alloc();
            st.prepare(Map::UpdateSql());
            Map::BindUpdate(st, entity);
            st.define_and_bind();
            st.execute(true);
        }
        catch (const std::exception& ex)
        {
            spdlog::error("Repository<{}>::Update: {}", Map::Table, ex.what());
            throw;
        }
    }

    template<typename PkType>
    void Delete(PkType pk)
    {
        try
        {
            m_sql << Map::DeleteSql(), soci::use(pk, "pk");
        }
        catch (const std::exception& ex)
        {
            spdlog::error("Repository<{}>::Delete: {}", Map::Table, ex.what());
            throw;
        }
    }

    // ── Upsert (INSERT or UPDATE if pk exists) ───────────────────────
    // Simple check-then-act; fine for low-contention paths. For high
    // concurrency, use a database-native MERGE or the server's own SP.
    void Upsert(const T& entity)
    {
        auto pk = Map::GetPk(entity);
        auto existing = FindById(pk);
        if (existing)
            Update(entity);
        else
            Insert(entity);
    }

    // ── Bulk operations (opt-in for cíleně chosen hot paths) ──────────
    //
    // These wrap N single-row statements in a single soci::transaction
    // — one COMMIT instead of N round-trips for the commit phase.
    // Typical speedup: 3-5× over the equivalent loop without a
    // transaction. NOT true multi-row VALUES bulk; for that path (40+×
    // speedup) write the INSERT directly via RawExecute() with a
    // pre-built `INSERT … VALUES (…), (…), …` string.
    //
    // When to use:
    //   ✓ TLogSvr drain loop popping N records off the retry queue
    //   ✓ SaveInventory dumping a full 32-slot inventory on logout
    //   ✓ CreateChar's batch of starter items
    //   ✓ Any operator command that touches a bounded N rows at once
    //
    // When NOT to use:
    //   ✗ Single-row CRUD — the transaction overhead is not worth it
    //   ✗ Reads — already set-based via Where() / RawSelect()
    //   ✗ Cross-table writes — Transaction(ctx.Session()) directly
    //     so the txn spans multiple statements / tables
    //
    // batch_size bounds how many rows per transaction; rows beyond the
    // batch land in a fresh transaction. Default 500 — keeps any one
    // commit's WAL / log volume manageable on a busy DB.
    void InsertAll(const std::vector<T>& rows, std::size_t batch_size = 500)
    {
        BatchedMutation(rows, batch_size,
            [this](const T& e) { Insert(e); }, "InsertAll");
    }

    void UpdateAll(const std::vector<T>& rows, std::size_t batch_size = 500)
    {
        BatchedMutation(rows, batch_size,
            [this](const T& e) { Update(e); }, "UpdateAll");
    }

    // DeleteAll by primary-key list. Single statement per row inside
    // a transaction; for very large deletes prefer a single
    // `RawExecute("DELETE FROM Table WHERE pk IN (...)")` instead.
    template<typename PkType>
    void DeleteAll(const std::vector<PkType>& pks, std::size_t batch_size = 500)
    {
        if (pks.empty()) return;
        for (std::size_t i = 0; i < pks.size(); i += batch_size)
        {
            const std::size_t end = std::min(i + batch_size, pks.size());
            soci::transaction tx(m_sql);
            try
            {
                for (std::size_t j = i; j < end; ++j)
                    Delete(pks[j]);
                tx.commit();
            }
            catch (const std::exception& ex)
            {
                try { tx.rollback(); } catch (...) {}
                spdlog::error("Repository<{}>::DeleteAll batch [{},{}): {}",
                    Map::Table, i, end, ex.what());
                throw;
            }
        }
    }

    // ── Raw SQL ───────────────────────────────────────────────────────
    // Escape hatch when the query doesn't fit the generic CRUD shape.
    std::vector<T> RawSelect(const std::string& sql)
    {
        return Query(sql);
    }

    // Execute a raw statement (INSERT / UPDATE / DELETE / EXEC).
    void RawExecute(const std::string& sql)
    {
        m_sql << sql;
    }

private:
    std::vector<T> Query(const std::string& sql)
    {
        std::vector<T> out;
        try
        {
            soci::rowset<soci::row> rs = (m_sql.prepare << sql);
            for (const auto& row : rs)
                out.push_back(Map::FromRow(row));
        }
        catch (const std::exception& ex)
        {
            spdlog::error("Repository<{}>::Query: {} sql={}",
                Map::Table, ex.what(), sql);
        }
        return out;
    }

    // Shared implementation of InsertAll / UpdateAll — chunks rows into
    // batches, wraps each batch in a single transaction, propagates the
    // first exception after rolling back the in-flight batch.
    template<typename PerRowFn>
    void BatchedMutation(const std::vector<T>& rows,
                         std::size_t batch_size,
                         PerRowFn&& per_row,
                         const char* op_name)
    {
        if (rows.empty()) return;
        if (batch_size == 0) batch_size = rows.size();
        for (std::size_t i = 0; i < rows.size(); i += batch_size)
        {
            const std::size_t end = std::min(i + batch_size, rows.size());
            soci::transaction tx(m_sql);
            try
            {
                for (std::size_t j = i; j < end; ++j)
                    per_row(rows[j]);
                tx.commit();
            }
            catch (const std::exception& ex)
            {
                try { tx.rollback(); } catch (...) {}
                spdlog::error("Repository<{}>::{} batch [{},{}): {}",
                    Map::Table, op_name, i, end, ex.what());
                throw;
            }
        }
    }

    soci::session& m_sql;
};

} // namespace fourstory::db::orm
