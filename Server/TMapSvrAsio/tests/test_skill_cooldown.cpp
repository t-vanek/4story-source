// Unit test: skill_cooldown.h — the reuse-cooldown gate (faithful
// CTSkill::CanUse / GetReuseRemainTick). Pure remain/can-use math + the
// per-(char, skill) tracker, all clock-free / deterministic.

#include "services/skill_cooldown.h"
#include "services/skill_timing.h"

#include <cstdint>
#include <cstdio>
#include <limits>
#include <thread>
#include <barrier>
#include <atomic>

namespace {
int g_fails = 0, g_checks = 0;
#define EXPECT(cond) do { ++g_checks; \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_fails; \
    } \
} while (0)
} // namespace

int main()
{
    using namespace tmapsvr;

    // Literal source timing arithmetic, including rank zero and DWORD wrap.
    {
        using namespace skill_timing;
        EXPECT(BaseAttackDelay(1500,2,1000,20,30)==898);
        EXPECT(BaseAttackDelay(500,2,1000,20,30)==500);
        EXPECT(BaseAttackDelay(1500,20,1000,20,30)==0);
        auto speed=AttackTiming(898,102,-20,25);
        EXPECT(speed.delay==1000&&speed.rate==60);
        EXPECT(AttackTiming(100,-200,0,0).delay==0);
        EXPECT(AttackTiming(0,0,-200,0).rate==0);
        EXPECT(AttackTiming(0,0,0,150).rate==0);
        SkillTemplate t;t.dwReuseDelay=1000;t.nReuseDelayInc=-100;
        EXPECT(ReuseDelay(t,3,{200,75})==750);
        EXPECT(ReuseDelay(t,0,{200,75})==975);
        t.dwReuseDelay=0xffffffffU;t.nReuseDelayInc=0;
        EXPECT(ReuseDelay(t,1,{0,100})==42949671);
        EXPECT(ReuseDelay(t,1,{1,100})==0);
        t.nReuseDelayInc=std::numeric_limits<std::int32_t>::max();
        bool rejected=false;try{(void)ReuseDelay(t,3,{});}catch(const std::domain_error&){rejected=true;}
        EXPECT(rejected);
        rejected=false;try{(void)BaseAttackDelay(1,std::numeric_limits<float>::quiet_NaN(),0,0,0);}catch(const std::domain_error&){rejected=true;}
        EXPECT(rejected);
    }
    // One cast arms its own and same-kind timers together. Peer durations
    // extend but never shorten; rejection cannot arm any other skill.
    {
        SkillCooldownTracker t;const std::vector<std::uint16_t> group{7,8,9};
        t.Restore(42,std::vector<SkillRow>{{8,1,100},{9,1,5000}},100);
        EXPECT(t.TryUse(42,7,150,1000,group,200));
        EXPECT(t.RemainMs(42,7,150)==1000&&t.RemainMs(42,8,150)==200&&t.RemainMs(42,9,150)==4950);
        const auto before=t.Snapshot(42,160);
        EXPECT(!t.TryUse(42,7,160,5000,group,6000)&&t.Snapshot(42,160)==before);
        EXPECT(t.TryUse(42,8,350,0,group,2000));
        EXPECT(t.RemainMs(42,7,350)==2000&&t.RemainMs(42,8,350)==2000&&t.RemainMs(42,9,350)==4750);
        EXPECT(t.RemainMs(42,10,350)==0&&t.RemainMs(99,7,350)==0);
    }
    {
        SkillCooldownTracker t;const std::vector<std::uint16_t> group{7,8};
        std::barrier gate(3);std::atomic<int> winners=0;
        auto cast=[&](std::uint16_t id){gate.arrive_and_wait();if(t.TryUse(1,id,0,1000,group,500))++winners;};
        std::thread a(cast,7),b(cast,8);gate.arrive_and_wait();a.join();b.join();
        EXPECT(winners==1&&t.Snapshot(1,0).size()==2);
    }

    // --- ReuseRemainMs --------------------------------------------------
    EXPECT(ReuseRemainMs(0,    1000, 5000) == 0);     // never used → ready
    EXPECT(ReuseRemainMs(1000, 1000, 5000) == 5000);  // just used → full
    EXPECT(ReuseRemainMs(1000, 3000, 5000) == 3000);  // 2000 elapsed → 3000 left
    EXPECT(ReuseRemainMs(1000, 6000, 5000) == 0);     // exactly elapsed → ready
    EXPECT(ReuseRemainMs(1000, 9000, 5000) == 0);     // long past → ready
    EXPECT(ReuseRemainMs(5000, 1000, 5000) == 0);     // clock backwards → ready
    EXPECT(ReuseRemainMs(1000, 1000, 0)    == 0);     // zero-delay → always ready

    // --- CanUseSkill ----------------------------------------------------
    EXPECT(CanUseSkill(0,    1000, 5000));            // never used
    EXPECT(!CanUseSkill(1000, 2000, 5000));           // 1000 elapsed < 5000
    EXPECT(CanUseSkill(1000, 6000, 5000));            // elapsed

    // --- SkillCooldownTracker -------------------------------------------
    {
        SkillCooldownTracker t;
        EXPECT(t.TryUse(42, 7, 1000, 5000));          // first use → OK (records 1000)
        EXPECT(!t.TryUse(42, 7, 2000, 5000));         // 1000 later → still cooling
        EXPECT(t.RemainMs(42, 7, 2000, 5000) == 4000);
        EXPECT(t.TryUse(42, 7, 6000, 5000));          // 5000 later → OK (records 6000)
        EXPECT(!t.TryUse(42, 7, 7000, 5000));         // cooling again

        // Independent per skill and per char.
        EXPECT(t.TryUse(42, 8, 2000, 5000));          // skill 8 ≠ skill 7
        EXPECT(t.TryUse(99, 7, 2000, 5000));          // char 99 ≠ char 42

        // Forget drops a char's stamps; others untouched.
        t.Forget(42);
        EXPECT(t.TryUse(42, 7, 6500, 5000));          // forgotten → ready
        EXPECT(!t.TryUse(99, 7, 2500, 5000));         // char 99 still cooling
    }

    {
        SkillCooldownTracker source,target;
        const std::vector<SkillRow> learned{{40000,7,300000},{7,2,1000},{8,1,0}};
        source.Restore(42,learned,0); // no process-clock zero sentinel
        EXPECT(!source.TryUse(42,7,0,1));
        EXPECT(source.RemainMs(42,40000,500,1)==299500); // template cannot truncate imported time
        auto exported=source.Snapshot(42,500);
        EXPECT((exported==std::vector<std::pair<std::uint16_t,std::uint32_t>>{{7,500},{40000,299500}}));
        EXPECT(source.Snapshot(42,500)==exported); // read does not rearm
        auto restored=learned;
        for(auto& row:restored){row.dwRemainTick=0;for(auto [id,remain]:exported)if(id==row.wSkillID)row.dwRemainTick=remain;}
        target.Restore(42,restored,9000000); // a different process clock
        EXPECT(target.RemainMs(42,7,9000499)==1);
        EXPECT(!target.TryUse(42,7,9000499,5000));
        EXPECT(target.TryUse(42,7,9000500,5000));
        EXPECT(target.RemainMs(42,7,9000501)==4999);
        EXPECT(target.RemainMs(42,40000,9000000)==299500);
        EXPECT(target.RemainMs(42,40000,8000000)==299500); // clock rollback cannot bypass
        auto invalid=restored;invalid.push_back(restored[0]);
        bool rejected=false;try{target.Restore(42,invalid,0);}catch(const std::invalid_argument&){rejected=true;}
        EXPECT(rejected&&target.RemainMs(42,40000,9000000)==299500); // atomic validation
        const std::vector<SkillRow> maximum{{65535,255,0xffffffff}};
        const auto near_max=std::numeric_limits<std::uint64_t>::max()-100;
        target.Restore(99,maximum,near_max);
        EXPECT(target.RemainMs(99,65535,near_max+50)==0xffffffff-50);
        EXPECT(!target.TryUse(99,65535,near_max+50,0));
        target.Restore(42,{},0);
        EXPECT(target.Snapshot(42,0).empty()&&!target.Snapshot(99,near_max).empty());
        target.Forget(99);EXPECT(target.Snapshot(99,near_max).empty());
        EXPECT(source.TryUse(42,7,1000,0)&&source.Snapshot(42,1000).size()==1);
    }
    {
        SkillCooldownTracker tracker;std::barrier gate(9);std::atomic<int> winners=0;
        std::vector<std::thread> writers;
        for(int i=0;i<8;++i)writers.emplace_back([&]{gate.arrive_and_wait();if(tracker.TryUse(1,2,0,5000))++winners;});
        gate.arrive_and_wait();for(auto& writer:writers)writer.join();
        EXPECT(winners==1&&tracker.RemainMs(1,2,0)==5000);
    }

    if (g_fails == 0)
        std::printf("test_skill_cooldown: %d checks passed\n",g_checks);
    return g_fails == 0 ? 0 : 1;
}
