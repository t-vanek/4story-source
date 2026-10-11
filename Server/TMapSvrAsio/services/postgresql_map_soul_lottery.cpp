#include "postgresql_map_service.h"
#include "legacy_random.h"
#include "MessageId.h"
#include <soci/soci.h>
#include <algorithm>

namespace tmapsvr {
bool PostgreSQLMapService::ChangeSoulLottery(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    bool settle,QuestPackets& packets) {
    using tnetlib::protocol::MessageId;
    // OnCS_SOULLOTTERY[_TRIGGER]_REQ, ordinary Map build. The native caller
    // already owns the primary session/Map and transaction. No source dead or
    // secure-code gate exists here. BOW's no-op belongs to its separate variant.
    auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
    if(!settle) {
        if(p->soul_exp!=static_cast<std::uint32_t>(p->next_exp-p->prev_exp))return false;
        std::array<std::uint32_t,4> numbers;
        for(auto& number:numbers)number=LegacyRandom15()%30U+1U;
        for(std::size_t i=1;i<numbers.size();++i)
            while(std::find(numbers.begin(),numbers.begin()+i,numbers[i])!=numbers.begin()+i)
                numbers[i]=LegacyRandom15()%30U+1U;
        const int level=s.bLevel;int stored=0;
        sql << "SELECT \"dwMoney\" FROM character_compat.\"TLEVELCHART\" WHERE \"bLevel\"=:level",
            soci::use(level),soci::into(stored);
        if(!sql.got_data()||!stored)return false;
        const auto base=static_cast<std::uint32_t>(stored);
        const auto range=static_cast<std::uint32_t>(base*(LegacyRandom15()%100U+1U));
        if(!range)throw std::runtime_error("Soul lottery source money range is zero after DWORD multiplication");
        const auto money=static_cast<std::uint32_t>(LegacyRandom15()%range+base);
        p->soul_exp=0;p->lottery_numbers=numbers;p->lottery_money=money;
        // The source keeps m_bUseLot set until settlement; a new qualifying
        // draw can overwrite an earlier pending draw. Do not add a pending gate.
        std::vector<std::byte> ack;
        for(const auto number:numbers)wire::WritePOD(ack,number);
        wire::WritePOD(ack,money);
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_SOULLOTTERY_ACK),std::move(ack));
    }else {
        if(!p->lottery_money||std::any_of(p->lottery_numbers.begin(),p->lottery_numbers.end(),[](auto n){return !n;}))return false;
        const auto number=LegacyRandom15()%30U+1U;
        const bool won=std::find(p->lottery_numbers.begin(),p->lottery_numbers.end(),number)!=p->lottery_numbers.end();
        const auto money=won?p->lottery_money:std::uint32_t{s.bLevel}*1000U;
        EarnNativeMoney(sql,c,s,money,packets);
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_MONEY_ACK),EncodeMoneyAck(s.dwGold,s.dwSilver,s.dwCooper));
        std::vector<std::byte> ack;wire::WritePOD(ack,number);wire::WritePOD(ack,money);
        wire::WritePOD(ack,static_cast<std::uint8_t>(won));
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_SOULLOTTERY_TRIGGER_ACK),std::move(ack));
        p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
        p->lottery_numbers={};p->lottery_money=0;p->soul_ready=false;
        ForceProgressEffect(sql,c,s,820,900000,packets); // (HOUR_ONE / 4) * 1000
    }
    // The existing quest operation head serializes draw and settlement against
    // older snapshots. Currency, titles, effect and response receipt commit
    // together. Lottery fields intentionally reset with CTPlayer admission:
    // LOADCHAR/SAVECHAR/RELEASEMAIN never persisted or transferred them. After an
    // unknown commit the session closes; no replay or new-login payout is made.
    return true;
}
}
