#pragma once

#include "event_types.h"
#include <bit>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace tcontrolsvr::event_value {

// CTControlSvrModule::MakeStrValue / ParseStrValue. Preserve the original
// delimited storage, including its documented restart quirks. Bad/truncated
// rows are refused instead of reading an uninitialized MONREGEN field.
inline std::vector<std::string_view> Tokens(std::string_view value,char delimiter) {
    std::vector<std::string_view> out;
    while(!value.empty()) {
        const auto pos=value.find(delimiter);
        const auto part=value.substr(0,pos);
        if(!part.empty())out.push_back(part); // CString::Tokenize skips empties.
        if(pos==std::string_view::npos)break;
        value.remove_prefix(pos+1);
    }
    return out;
}
inline std::uint32_t Integer(std::string_view text) {
    // Source atoi is signed 32-bit; retain the resulting DWORD bits. Overflow
    // and non-numeric data had no defined contract and are rejected.
    std::int32_t value=0;
    const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),value);
    if(error!=std::errc{}||end!=text.data()+text.size())throw std::runtime_error("Invalid stored event integer");
    return std::bit_cast<std::uint32_t>(value);
}
inline float Real(std::string_view text) {
    float value=0;
    const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),value);
    if(error!=std::errc{}||end!=text.data()+text.size()||!std::isfinite(value))
        throw std::runtime_error("Invalid stored event coordinate");
    return value;
}
inline std::string Encode(const EventInfo& ev) {
    std::ostringstream out;out.imbue(std::locale::classic());
    switch(ev.kind) {
    case event_kind::kCashSale:
        for(const auto& item:ev.cash_items)out<<item.item_id<<'-'<<unsigned(item.sale_value)<<';';
        break;
    case event_kind::kMonSpawn:
        for(const auto id:ev.mon_event.spawn_ids)out<<id<<';';
        break;
    case event_kind::kMonRegen:
        out<<std::fixed<<std::setprecision(2);
        for(const auto& m:ev.mon_regens) {
            if(!std::isfinite(m.pos_x)||!std::isfinite(m.pos_y)||!std::isfinite(m.pos_z))
                throw std::runtime_error("Invalid event coordinate");
            out<<m.mon_id<<'-'<<std::bit_cast<std::int32_t>(m.delay)<<'-'<<m.map_id<<'-'
                <<m.pos_x<<'-'<<m.pos_y<<'-'<<m.pos_z<<';';
        }
        break;
    case event_kind::kLottery:
        for(const auto& item:ev.lotteries)out<<item.item_id<<'x'<<unsigned(item.count)<<'-'<<item.winner<<';';
        out<<'|'<<ev.lottery_msg;break;
    case event_kind::kGiftTime:out<<ev.lottery_msg;break;
    default:break; // Source has no szValue for scalar event types.
    }
    auto value=out.str();
    if(value.size()>1024||value.find('\0')!=std::string::npos)
        throw std::runtime_error("Event value exceeds original storage contract");
    return value;
}
inline void Decode(EventInfo& ev,std::string_view value) {
    ev.cash_items.clear();ev.mon_event={};ev.mon_regens.clear();ev.lotteries.clear();ev.lottery_msg.clear();
    switch(ev.kind) {
    case event_kind::kCashSale:
        for(const auto entry:Tokens(value,';')) {
            const auto fields=Tokens(entry,'-');
            if(fields.size()!=2)throw std::runtime_error("Invalid stored cash sale");
            ev.cash_items.push_back({static_cast<std::uint16_t>(Integer(fields[0])),static_cast<std::uint8_t>(Integer(fields[1]))});
        }
        break;
    case event_kind::kMonSpawn:
        for(const auto entry:Tokens(value,';'))ev.mon_event.spawn_ids.push_back(static_cast<std::uint16_t>(Integer(entry)));
        break;
    case event_kind::kMonRegen:
        for(const auto entry:Tokens(value,';')) {
            // Source '-' tokenizer drops negative signs. Do not silently
            // invent a different historical interpretation of such rows.
            const auto f=Tokens(entry,'-');
            if(f.size()!=6)throw std::runtime_error("Invalid stored monster regeneration");
            ev.mon_regens.push_back({static_cast<std::uint16_t>(Integer(f[0])),Integer(f[1]),
                static_cast<std::uint16_t>(Integer(f[2])),Real(f[3]),Real(f[4]),Real(f[5])});
        }
        break;
    case event_kind::kLottery: {
        const auto split=value.find('|');
        if(split==std::string_view::npos)throw std::runtime_error("Invalid stored lottery");
        const auto rewards=value.substr(0,split);
        const auto entries=Tokens(rewards,';');
        if(!entries.empty()) {
            // Original ParseStrValue reads strTok (the entire rewards prefix)
            // rather than strTok2. Consequently every restored row starts
            // with the first reward's item/count/winner; preserve that quirk.
            const auto x=rewards.find('x'),dash=rewards.find('-');
            if(x==std::string_view::npos||dash==std::string_view::npos||dash<=x)
                throw std::runtime_error("Invalid stored lottery reward");
            for(const auto entry:entries) {
                if(entry.size()<=dash+1)throw std::runtime_error("Invalid stored lottery reward length");
                auto winner=rewards.substr(dash+1,entry.size()-dash-1);
                // atoi stops at the first ';' in the original whole prefix.
                winner=winner.substr(0,winner.find(';'));
                ev.lotteries.push_back({static_cast<std::uint16_t>(Integer(rewards.substr(0,x))),
                    static_cast<std::uint8_t>(Integer(rewards.substr(x+1,dash-x-1))),
                    static_cast<std::uint16_t>(Integer(winner))});
            }
        }
        ev.lottery_msg=std::string(value.substr(split+1));break;
    }
    default:break; // Source also intentionally omits GIFTTIME restoration.
    }
}
} // namespace tcontrolsvr::event_value
