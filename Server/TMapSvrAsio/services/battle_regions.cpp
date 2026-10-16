#include "battle_regions.h"
#include <stdexcept>

namespace tmapsvr {
bool BattleRegions::CanTalk(std::uint8_t country,std::uint8_t aid_country,std::uint8_t disguise,
    std::uint8_t npc_country,std::uint16_t npc_local) const {
    // TNpc.cpp::CanTalk and the boot-time FindOccupationZone binding. Local0,
    // missing, invalid and non-occupation objects leave the NPC unbound.
    const bool neutral=npc_country==3;
    if(neutral&&npc_local) {
        const auto local=locals.find(npc_local);
        if(local!=locals.end()&&local->second.type==LocalType::Occupation&&local->second.valid)
            npc_country=local->second.country;
    }
    const bool friendly=country==npc_country||aid_country==npc_country;
    if(!neutral)return friendly&&(!disguise||disguise==npc_country+4);
    return npc_country==3||(friendly&&(!disguise||disguise==npc_country+4))||
        (!friendly&&disguise==npc_country+4);
}
std::vector<std::byte> BattleRegions::HeroList() const {
    std::vector<std::byte> packet;
    const auto byte=[&](std::uint8_t n){packet.push_back(static_cast<std::byte>(n));};
    const auto little=[&](std::uint32_t n,unsigned width){
        for(unsigned shift=0;shift<width*8;shift+=8)byte(static_cast<std::uint8_t>(n>>shift));
    };
    // CSSender.cpp::SendCS_HEROLIST_ACK: all occupation rows, then castles;
    // no validity, tutorial, guild or empty-name filtering in this reader.
    for(const auto type:{LocalType::Occupation,LocalType::Castle}) {
        const auto& ids=order.at(static_cast<std::size_t>(type)-1);
        if(ids.size()>255)throw std::runtime_error("Source HEROLIST BYTE count overflow");
        byte(static_cast<std::uint8_t>(ids.size()));
        for(const auto id:ids) {
            const auto& region=locals.at(id);
            if(region.type!=type||region.hero.size()>50||region.hero.find('\0')!=std::string::npos)
                throw std::runtime_error("Invalid actual battle-region hero state");
            little(id,2);little(static_cast<std::uint32_t>(region.hero.size()),4);
            for(const unsigned char ch:region.hero)byte(ch);
        }
    }
    return packet;
}
} // namespace tmapsvr
