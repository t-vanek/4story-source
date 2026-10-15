#pragma once
#include "domain/character.h"

namespace tmapsvr {
inline bool NativeRecoveryWork(const CharSnapshot& s) {
    if(!s.recovery)return false;
    // The source battle timeout runs even for a dead actor or full resources.
    if(s.cluster.mode==1)return true;
    return !s.bDead&&s.payload&&s.payload->recovery_rates&&
        ((s.cluster.mode==0&&s.dwHP<s.dwMaxHP)||s.dwMP<s.dwMaxMP);
}
}
