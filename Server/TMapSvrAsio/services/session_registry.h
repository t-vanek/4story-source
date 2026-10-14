#pragma once

#include "asio_session.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <limits>
#include <stdexcept>
#include "domain/session.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tmapsvr {

enum class WorldPresence { Unannounced, Announced, Retired };

enum class SessionPhase { Pending, Loading, Loaded, Admitted, Ready, TransferOut, TransferIn, Closing, Rejected };
struct SessionIdentity {
    std::uint32_t char_id{}, user_id{}, key{};
    std::uint8_t channel{};
    SessionPhase phase{SessionPhase::Pending};
    std::uint64_t connection_id{};
    WorldPresence world_presence{WorldPresence::Unannounced};
    MapSessionRole role{MapSessionRole::Primary};
    std::uint32_t endpoint_ip{};
    std::uint16_t endpoint_port{};
    std::uint64_t authority_epoch{};
    MapSessionClaim Claim(std::uint8_t group) const { return {user_id,key,char_id,group,channel,connection_id,role,endpoint_ip,endpoint_port,authority_epoch}; }
};
struct SourceServiceData {std::uint32_t players{},active_users{};};

// A reservation remains owned until disconnect persistence completes. A new
// socket may never replace a live reservation, even when its socket is closed.
class ISessionRegistry {
public:
    virtual ~ISessionRegistry() = default;
    virtual bool TryBind(SessionIdentity identity,
        std::shared_ptr<tnetlib::AsioSession> session) = 0;
    virtual void Unbind(std::uint32_t char_id) = 0;
    virtual std::size_t UnbindIfMatches(const tnetlib::AsioSession* session) = 0;
    virtual std::shared_ptr<tnetlib::AsioSession> Find(std::uint32_t char_id) const = 0;
    virtual std::shared_ptr<tnetlib::AsioSession> Find(std::uint32_t char_id,
        std::uint32_t key) const = 0;
    virtual std::optional<SessionIdentity> Identity(const tnetlib::AsioSession* session) const = 0;
    // Socket identity fences the flag against replacement connections. World
    // retirement never changes the local phase: a ready native claim must save.
    virtual bool SetWorldPresence(const tnetlib::AsioSession*, WorldPresence) = 0;
    virtual bool SetRole(const tnetlib::AsioSession*, MapSessionRole) = 0;
    virtual bool BeginTransfer(const tnetlib::AsioSession*, MapSessionRole) = 0;
    virtual bool FinishTransfer(const tnetlib::AsioSession*, MapSessionRole, std::uint64_t epoch) = 0;
    virtual void EndOperation(const tnetlib::AsioSession*) = 0;
    virtual bool HasOperation(const tnetlib::AsioSession*) const = 0;
    virtual unsigned Operations(const tnetlib::AsioSession*) const = 0;
    virtual bool BeginGameplay(const tnetlib::AsioSession*) = 0;
    virtual bool BeginCheckpoint(const tnetlib::AsioSession*) = 0;
    // Read-only native World projections may arrive before client CONREADY.
    // They serialize with admission, gameplay and final disconnect persistence.
    virtual bool BeginProjection(const tnetlib::AsioSession* session) {return BeginCheckpoint(session);}
    // Atomic with BeginTransfer: teardown cannot race a new ownership operation.
    virtual std::optional<SessionIdentity> BeginClose(const tnetlib::AsioSession*) = 0;
    virtual bool Transition(std::uint32_t char_id, std::uint32_t key,
        SessionPhase from, SessionPhase to) = 0;
    virtual std::optional<std::uint32_t> FindCharIdBySession(
        const tnetlib::AsioSession* session) const = 0;
    virtual std::size_t Size() const = 0;
    virtual bool MarkAccepted(const tnetlib::AsioSession*) {throw std::runtime_error("Source accepted registry unavailable");}
    virtual bool Accepted(const tnetlib::AsioSession*) const {throw std::runtime_error("Source accepted registry unavailable");}
    virtual SourceServiceData ServiceData() const {throw std::runtime_error("Source service statistics unavailable");}
    virtual void ClearServiceData() {throw std::runtime_error("Source active-user registry unavailable");}
    virtual std::vector<std::pair<SessionIdentity,std::shared_ptr<tnetlib::AsioSession>>> ReadyPrimaries() const {
        throw std::runtime_error("Communication primary registry unavailable");
    }
    virtual std::vector<std::pair<SessionIdentity,std::shared_ptr<tnetlib::AsioSession>>> ReadyPlayers() const {
        throw std::runtime_error("Communication local player registry unavailable");
    }
    virtual std::vector<std::pair<SessionIdentity,std::shared_ptr<tnetlib::AsioSession>>> PrimarySessions() const {
        throw std::runtime_error("Communication admission registry unavailable");
    }
};

class InMemorySessionRegistry final : public ISessionRegistry {
public:
    bool TryBind(SessionIdentity id, std::shared_ptr<tnetlib::AsioSession> session) override {
        if (!session || !id.char_id || !id.user_id || !id.key) return false;
        std::lock_guard lock(m_mtx);
        for (const auto& [cid, row] : m_rows)
            if (auto current = row.session.lock(); current &&
                (cid == id.char_id || current.get() == session.get())) return false;
        if (m_generation==static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) return false;
        id.connection_id=++m_generation;
        m_rows[id.char_id] = Row{id, std::move(session)};
        return true;
    }
    void Unbind(std::uint32_t char_id) override {
        std::lock_guard lock(m_mtx); m_rows.erase(char_id);
    }
    std::size_t UnbindIfMatches(const tnetlib::AsioSession* session) override {
        std::lock_guard lock(m_mtx);
        std::size_t removed = 0;
        for (auto it = m_rows.begin(); it != m_rows.end();) {
            if (auto current = it->second.session.lock(); current && current.get() == session) {
                it = m_rows.erase(it); ++removed;
            } else ++it;
        }
        return removed;
    }
    std::shared_ptr<tnetlib::AsioSession> Find(std::uint32_t char_id) const override {
        std::lock_guard lock(m_mtx);
        const auto it = m_rows.find(char_id);
        return it == m_rows.end() ? nullptr : it->second.session.lock();
    }
    std::shared_ptr<tnetlib::AsioSession> Find(std::uint32_t char_id, std::uint32_t key) const override {
        std::lock_guard lock(m_mtx);
        const auto it = m_rows.find(char_id);
        if (it == m_rows.end() || it->second.id.key != key ||
            it->second.id.phase == SessionPhase::Closing ||
            it->second.id.phase == SessionPhase::Rejected) return nullptr;
        return it->second.session.lock();
    }
    std::optional<SessionIdentity> Identity(const tnetlib::AsioSession* session) const override {
        if (!session) return std::nullopt;
        std::lock_guard lock(m_mtx);
        for (const auto& [cid, row] : m_rows)
            if (auto current = row.session.lock(); current && current.get() == session) return row.id;
        return std::nullopt;
    }
    bool SetWorldPresence(const tnetlib::AsioSession* session, WorldPresence presence) override {
        std::lock_guard lock(m_mtx);
        for (auto& [cid, row] : m_rows)
            if (auto current = row.session.lock(); current && current.get() == session) {
                // An asynchronous sender may finish after World retired it.
                if (row.id.world_presence == WorldPresence::Retired) return false;
                row.id.world_presence = presence; return true;
            }
        return false;
    }
    bool SetRole(const tnetlib::AsioSession* session, MapSessionRole role) override {
        std::lock_guard lock(m_mtx);
        for (auto& [cid, row] : m_rows)
            if (auto current = row.session.lock(); current && current.get() == session) {
                if (row.id.phase != SessionPhase::Pending || row.id.world_presence != WorldPresence::Unannounced) return false;
                row.id.role = role; return true;
            }
        return false;
    }
    bool BeginTransfer(const tnetlib::AsioSession* session,MapSessionRole role) override {
        std::lock_guard lock(m_mtx);
        for(auto& [cid,row]:m_rows)if(auto current=row.session.lock();current&&current.get()==session) {
            if(row.id.phase!=SessionPhase::Ready||row.id.role!=role)return false;
            row.id.phase=role==MapSessionRole::Primary?SessionPhase::TransferOut:SessionPhase::TransferIn;
            ++row.operations;return true;
        }
        return false;
    }
    bool FinishTransfer(const tnetlib::AsioSession* session,MapSessionRole role,std::uint64_t epoch) override {
        std::lock_guard lock(m_mtx);
        for(auto& [cid,row]:m_rows)if(auto current=row.session.lock();current&&current.get()==session) {
            const auto expected=role==MapSessionRole::Primary?SessionPhase::TransferIn:SessionPhase::TransferOut;
            if(row.id.phase!=expected||epoch<=row.id.authority_epoch)return false;
            row.id.role=role;row.id.authority_epoch=epoch;
            row.id.phase=role==MapSessionRole::Primary?SessionPhase::Loaded:SessionPhase::Ready;
            return true;
        }
        return false;
    }
    void EndOperation(const tnetlib::AsioSession* session) override {
        std::lock_guard lock(m_mtx);
        for(auto& [cid,row]:m_rows)if(auto current=row.session.lock();current&&current.get()==session) {
            if(row.operations)--row.operations;
            if(!row.operations)row.checkpoint=false;
            return;
        }
    }
    bool HasOperation(const tnetlib::AsioSession* session) const override {
        return Operations(session)!=0;
    }
    unsigned Operations(const tnetlib::AsioSession* session) const override {
        std::lock_guard lock(m_mtx);
        for(const auto& [cid,row]:m_rows)if(auto current=row.session.lock();current&&current.get()==session)return row.operations;
        return 0;
    }
    bool BeginGameplay(const tnetlib::AsioSession* session) override {
        std::lock_guard lock(m_mtx);
        for(auto& [cid,row]:m_rows)if(auto current=row.session.lock();current&&current.get()==session) {
            if(row.id.phase!=SessionPhase::Ready||row.checkpoint)return false;
            ++row.operations;return true;
        }
        return false;
    }
    bool BeginCheckpoint(const tnetlib::AsioSession* session) override {
        std::lock_guard lock(m_mtx);
        for(auto& [cid,row]:m_rows)if(auto current=row.session.lock();current&&current.get()==session) {
            if(row.id.phase!=SessionPhase::Ready||row.id.role!=MapSessionRole::Primary||row.operations)return false;
            row.checkpoint=true;++row.operations;return true;
        }
        return false;
    }
    bool BeginProjection(const tnetlib::AsioSession* session) override {
        std::lock_guard lock(m_mtx);
        for(auto& [cid,row]:m_rows)if(auto current=row.session.lock();current&&current.get()==session) {
            if(row.id.role!=MapSessionRole::Primary||row.operations||
               (row.id.phase!=SessionPhase::Loaded&&row.id.phase!=SessionPhase::Admitted&&row.id.phase!=SessionPhase::Ready))return false;
            row.checkpoint=true;++row.operations;return true;
        }
        return false;
    }
    std::optional<SessionIdentity> BeginClose(const tnetlib::AsioSession* session) override {
        std::lock_guard lock(m_mtx);
        for(auto& [cid,row]:m_rows)if(auto current=row.session.lock();current&&current.get()==session) {
            if(row.operations)return {};
            const auto before=row.id;row.id.phase=SessionPhase::Closing;return before;
        }
        return {};
    }
    bool Transition(std::uint32_t char_id, std::uint32_t key,
        SessionPhase from, SessionPhase to) override {
        std::lock_guard lock(m_mtx);
        const auto it = m_rows.find(char_id);
        if (it == m_rows.end() || it->second.id.key != key ||
            it->second.id.phase != from || it->second.session.expired()) return false;
        it->second.id.phase = to; return true;
    }
    std::optional<std::uint32_t> FindCharIdBySession(const tnetlib::AsioSession* session) const override {
        if (auto id = Identity(session)) return id->char_id;
        return std::nullopt;
    }
    std::size_t Size() const override { std::lock_guard lock(m_mtx); return m_rows.size(); }
    bool MarkAccepted(const tnetlib::AsioSession* session) override {
        std::lock_guard lock(m_mtx);
        for(auto& [cid,row]:m_rows)if(auto current=row.session.lock();current&&current.get()==session) {
            if(row.id.phase!=SessionPhase::Pending)return false;
            row.accepted=true;m_active_users.insert(row.id.user_id);return true;
        }
        return false;
    }
    bool Accepted(const tnetlib::AsioSession* session) const override {
        std::lock_guard lock(m_mtx);
        for(const auto& [cid,row]:m_rows)if(auto current=row.session.lock();current&&current.get()==session)return row.accepted;
        return false;
    }
    SourceServiceData ServiceData() const override {
        std::lock_guard lock(m_mtx);SourceServiceData data;
        for(const auto& [cid,row]:m_rows)if(row.accepted&&!row.session.expired())++data.players;
        data.active_users=static_cast<std::uint32_t>(m_active_users.size());return data;
    }
    void ClearServiceData() override {
        std::lock_guard lock(m_mtx);m_active_users.clear();
        for(const auto& [cid,row]:m_rows)if(row.accepted&&!row.session.expired())m_active_users.insert(row.id.user_id);
    }
    std::vector<std::pair<SessionIdentity,std::shared_ptr<tnetlib::AsioSession>>> ReadySessions() const {
        std::lock_guard lock(m_mtx);
        std::vector<std::pair<SessionIdentity,std::shared_ptr<tnetlib::AsioSession>>> out;
        for(const auto& [cid,row]:m_rows)
            if(row.id.phase==SessionPhase::Ready && row.id.role==MapSessionRole::Primary)
                if(auto session=row.session.lock())out.emplace_back(row.id,std::move(session));
        return out;
    }
    std::vector<std::pair<SessionIdentity,std::shared_ptr<tnetlib::AsioSession>>> ReadyPrimaries() const override {return ReadySessions();}
    std::vector<std::pair<SessionIdentity,std::shared_ptr<tnetlib::AsioSession>>> ReadyPlayers() const override {
        std::lock_guard lock(m_mtx);
        std::vector<std::pair<SessionIdentity,std::shared_ptr<tnetlib::AsioSession>>> out;
        for(const auto& [cid,row]:m_rows)if(row.id.phase==SessionPhase::Ready)
            if(auto session=row.session.lock())out.emplace_back(row.id,std::move(session));
        return out;
    }
    std::vector<std::pair<SessionIdentity,std::shared_ptr<tnetlib::AsioSession>>> PrimarySessions() const override {
        std::lock_guard lock(m_mtx);
        std::vector<std::pair<SessionIdentity,std::shared_ptr<tnetlib::AsioSession>>> out;
        for(const auto& [cid,row]:m_rows)if(row.id.role==MapSessionRole::Primary&&
            (row.id.phase==SessionPhase::Loaded||row.id.phase==SessionPhase::Admitted||row.id.phase==SessionPhase::Ready))
            if(auto session=row.session.lock())out.emplace_back(row.id,std::move(session));
        return out;
    }
private:
    struct Row { SessionIdentity id; std::weak_ptr<tnetlib::AsioSession> session; unsigned operations=0; bool checkpoint=false,accepted=false; };
    std::uint64_t m_generation=0;
    mutable std::mutex m_mtx;
    std::unordered_map<std::uint32_t, Row> m_rows;
    // Original m_mapACTIVEUSER records unique accepted accounts until a
    // Control DATACLEAR rebuild. Logout alone never removes an entry.
    std::unordered_set<std::uint32_t> m_active_users;
};

class SessionOperation final {
public:
    SessionOperation(ISessionRegistry& registry,const tnetlib::AsioSession* session):m_registry(&registry),m_session(session){}
    SessionOperation(const SessionOperation&)=delete;
    ~SessionOperation(){Finish();}
    void Finish(){if(m_registry){m_registry->EndOperation(m_session);m_registry=nullptr;}}
private:
    ISessionRegistry* m_registry;const tnetlib::AsioSession* m_session;
};

} // namespace tmapsvr
