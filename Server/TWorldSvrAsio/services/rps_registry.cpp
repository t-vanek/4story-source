#include "rps_registry.h"

#include <cstdint>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace tworldsvr {

namespace {

// Legacy DAY_ONE = 86400 seconds; the 30-day expiry window in
// OnMW_RPSGAME_ACK is `30 * DAY_ONE` (SSHandler.cpp:13208).
constexpr std::int64_t kSecondsPerDay = 86400;
constexpr std::int64_t kExpiryWindowDays = 30;

RpsRegistry::Outcome Evaluate(TRpsGame& entry,std::uint32_t character,
    std::int64_t now,std::vector<RpsRegistry::PersistOp>& operations) {
    if(!entry.win_keep)return RpsRegistry::Outcome::kAllowed;
    // Source WORD indexing can wrap on a larger vector; do not reproduce its
    // infinite loop or stale copied-vector erase. No partial mutation on refusal.
    if(entry.win_dates.size()>std::numeric_limits<std::uint16_t>::max())
        throw std::runtime_error("RPS source WORD history domain exceeded");
    // WORD promotes to Win32 INT in wWinPeriod*DAY_ONE. Widening an
    // overflowing source expression would silently introduce a new game rule.
    if(entry.win_period>std::numeric_limits<std::int32_t>::max()/kSecondsPerDay)
        throw std::runtime_error("RPS source signed period product is undefined");
    const auto window=static_cast<std::int64_t>(entry.win_period)*kSecondsPerDay;
    std::uint16_t fresh=0;
    for(auto i=entry.win_dates.begin();i!=entry.win_dates.end();) {
        // The source SMALLDATETIME/clock domain is checked before native use.
        const auto age=now-*i;
        if(age<window) {fresh=static_cast<std::uint16_t>(fresh+1U);++i;}
        else if(age>kExpiryWindowDays*kSecondsPerDay) {
            operations.push_back({false,0,entry.type,entry.win_count,*i});
            i=entry.win_dates.erase(i);
        } else ++i;
    }
    if(fresh>=entry.win_keep)return RpsRegistry::Outcome::kCapReached;
    entry.win_dates.push_back(now);
    operations.push_back({true,character,entry.type,entry.win_count,now});
    return RpsRegistry::Outcome::kAllowed;
}

} // namespace

boost::asio::awaitable<RpsRegistry::OperationLease> RpsRegistry::AcquireOperation() {
    const auto executor=co_await boost::asio::this_coro::executor;
    if(!m_operation) {
        m_operation=std::make_shared<OperationPermit>(executor,1);
        m_operation->try_send(boost::system::error_code{},0);
    }
    co_await m_operation->async_receive(boost::asio::use_awaitable);
    co_return OperationLease(m_operation);
}

RpsRegistry::WinPlan RpsRegistry::PlanWin(std::uint8_t type,std::uint8_t count,
    std::uint32_t character,std::int64_t now) const {
    std::shared_lock guard(m_lock);WinPlan plan;
    const auto it=m_games.find(Key(type,count));
    if(it==m_games.end())return plan;
    plan.before=plan.after=it->second;
    plan.outcome=Evaluate(plan.after,character,now,plan.operations);
    return plan;
}
bool RpsRegistry::PublishWin(const WinPlan& plan) {
    std::unique_lock guard(m_lock);
    if(plan.outcome==Outcome::kNotFound)return true;
    const auto it=m_games.find(Key(plan.before.type,plan.before.win_count));
    if(it==m_games.end()||it->second!=plan.before)return false;
    it->second=plan.after;return true;
}

RpsRegistry::Outcome
RpsRegistry::RecordWin(std::uint8_t type, std::uint8_t win_count,
                       std::uint32_t char_id, std::int64_t now,
                       std::vector<PersistOp>& out_ops)
{
    std::unique_lock g(m_lock);
    auto it = m_games.find(Key(type, win_count));
    if (it == m_games.end()) return Outcome::kNotFound;

    return Evaluate(it->second,char_id,now,out_ops);
}

bool
RpsRegistry::Set(std::uint8_t type, std::uint8_t win_count,
                 std::uint8_t win_prob, std::uint8_t draw_prob,
                 std::uint8_t lose_prob, std::uint16_t win_keep,
                 std::uint16_t win_period)
{
    std::unique_lock g(m_lock);
    auto it = m_games.find(Key(type, win_count));
    if (it == m_games.end()) return false;
    it->second.win_prob   = win_prob;
    it->second.draw_prob  = draw_prob;
    it->second.lose_prob  = lose_prob;
    it->second.win_keep   = win_keep;
    it->second.win_period = win_period;
    return true;
}

bool
RpsRegistry::Insert(const TRpsGame& game)
{
    std::unique_lock g(m_lock);
    return m_games.emplace(Key(game.type, game.win_count), game).second;
}

std::vector<TRpsGame>
RpsRegistry::Snapshot() const
{
    std::shared_lock g(m_lock);
    std::vector<TRpsGame> out;
    out.reserve(m_games.size());
    for (const auto& [_, game] : m_games)
        out.push_back(game);
    std::sort(out.begin(),out.end(),[](const auto& a,const auto& b){
        return Key(a.type,a.win_count)<Key(b.type,b.win_count);
    });
    return out;
}

std::size_t
RpsRegistry::Size() const
{
    std::shared_lock g(m_lock);
    return m_games.size();
}

bool RpsRegistry::HydrateWinDate(std::uint8_t type,
                                 std::uint8_t win_count,
                                 std::int64_t date_unix)
{
    std::unique_lock g(m_lock);
    auto it = m_games.find(Key(type, win_count));
    if (it == m_games.end())
        return false;
    it->second.win_dates.push_back(date_unix);
    return true;
}

} // namespace tworldsvr
