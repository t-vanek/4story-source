#include "battle_scheduler.h"
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace tworldsvr {
namespace {
constexpr std::uint32_t day=86400;
constexpr std::uint8_t normal=0,battle_status=1,peace=4; // NetCode.h BATTLE_STATUS
void Clock(const BattleClock& c) {
    if(c.second<0||c.second>4000000000LL||c.day_second>=day||c.weekday<1||c.weekday>7)
        throw std::runtime_error("Source battle calendar outside supported domain");
}
std::uint32_t Left(std::uint32_t now,std::uint32_t end) {
    return now>end?day-now+end:end-now; // source DWORD operations, no normalization
}
}
void BattleScheduler::Load(const std::vector<BattleTime>& rows,std::vector<std::uint32_t> missions,const BattleClock& clock) {
    Clock(clock);
    if(missions.size()>255)throw std::runtime_error("Source mission BYTE iteration overflow");
    std::array<BattleTime,battle::kTypeCount> times{};std::set<std::uint8_t> keys;
    for(auto row:rows) {
        if(row.type>=battle::kTypeCount)continue;
        if(!keys.insert(row.type).second)throw std::runtime_error("Duplicate source battle type");
        row.status=normal;row.running=false;row.left=0;row.loaded=true;
        if(row.type==battle::kTypeMission&&row.start) {
            std::map<std::uint32_t,std::uint32_t> close;
            for(auto time:missions)close.emplace(time-clock.day_second,time);
            if(close.empty())throw std::runtime_error("Source mission boot dereferences an empty custom-time map");
            const auto closest=close.begin()->second;
            if(closest<clock.day_second) {
                const auto chosen=close.find(2);
                if(chosen==close.end())throw std::runtime_error("Source mission boot dereferences missing delta2");
                row.start=chosen->second;
            }else row.start=closest;
            if(!row.start)row.start=closest;
        }
        times[row.type]=row;
    }
    times[battle::kTypeLocal].peace=180; // literal source override after chart load
    m_times=times;m_missions=std::move(missions);m_loaded=true;
}
std::optional<BattleSignal> BattleScheduler::Tick(const BattleClock& clock) {
    if(!m_loaded)throw std::runtime_error("Battle scheduler has no source initialization");
    Clock(clock);auto times=m_times;const auto now=clock.day_second;
    auto selected=battle::kTypeLocal;
    const auto& sky=times[battle::kTypeSkyGarden];
    if(times[battle::kTypeCastle].day==clock.weekday)selected=battle::kTypeCastle;
    else if(now>=sky.start-sky.alarm_start-std::uint32_t{60}&&
            now<=sky.start+sky.duration+sky.peace+std::uint32_t{60})selected=battle::kTypeSkyGarden;
    if(!times[selected].running) {
        const auto& mission=times[battle::kTypeMission];
        if(mission.running)selected=battle::kTypeMission;
        else if(mission.start) {
            const std::uint32_t start=mission.start-mission.alarm_start-std::uint32_t{60};
            const std::uint32_t end=mission.start+mission.duration+mission.alarm_end+std::uint32_t{60};
            if((now>=start-day&&now<=end-day)||(now>=start&&now<=end))selected=battle::kTypeMission;
        }
    }
    auto& time=times[selected];if(!time.start)return {};
    const auto previous=time.status;bool alarm=false;std::uint32_t left=0;
    const auto start_left=Left(now,time.start),end_left=Left(now,(time.start+time.duration)%day);
    time.left=end_left;
    switch(time.status) {
    case normal:
        if(start_left<=time.alarm_start) {
            const std::uint32_t interval=start_left>600?600:start_left>60?120:10;
            if(start_left&&start_left%interval==0){left=start_left;alarm=true;time.running=true;}
        }else if(end_left<=time.duration){time.status=battle_status;left=end_left;time.running=true;}
        break;
    case battle_status:
        if(end_left>time.duration)time.status=peace;
        else if(end_left&&end_left<=time.alarm_end) {
            const std::uint32_t interval=end_left>60?60:10;
            if(end_left%interval==0){left=end_left;alarm=true;}
        }
        break;
    case peace:
        if(Left(now,(time.start+time.duration+time.peace)%day)>time.peace) {
            time.status=normal;time.running=false;
            if(time.type==battle::kTypeMission)for(std::size_t i=0;i<m_missions.size();++i)
                if(m_missions[i]==time.start||m_missions[i]==time.start-day) {
                    ++i;
                    if(i==m_missions.size())time.start=m_missions.front()+day;
                    else if(m_missions[i]!=time.start)time.start=m_missions[i];
                    else {
                        if(++i==m_missions.size())throw std::runtime_error("Source next mission reads past duplicate tail");
                        time.start=m_missions[i];
                    }
                    break;
                }
        }
        break;
    default:break;
    }
    std::optional<BattleSignal> signal;
    if(alarm||time.status!=previous) {
        if(time.status==peace)left=time.peace;
        signal=BattleSignal{time.type,time.status,time.start,left};
    }
    m_times=times;return signal;
}
}
