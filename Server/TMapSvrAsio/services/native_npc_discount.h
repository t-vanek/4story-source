#pragma once
#include <cstdint>
#include <stdexcept>

namespace tmapsvr {
inline std::uint8_t NpcDiscount(int condition,int rate,int local) {
    // CTMapSvrModule::GetDiscountRate: an absent local can never grant the
    // guild discount; unrecognised conditions explicitly leave the rate zero.
    // Hero lookup needs the live local/castle ownership registry. Its missing
    // authority must not silently become a successful full-price operation.
    if(!rate||condition<1||condition>3||(condition==1&&!local))return 0;
    throw std::runtime_error("NPC guild/hero discount authority is not integrated");
}
}
