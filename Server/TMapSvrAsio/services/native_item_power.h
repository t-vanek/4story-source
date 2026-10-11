#pragma once
#include "domain/character.h"
#include <soci/soci.h>

namespace tmapsvr {
// CTItem::GetPowerLevel, including SetItemAttr's unsigned WORD fallback.
std::uint8_t NativeItemPowerLevel(soci::session&,const ItemInstance&,const soci::row& item_chart);
}
