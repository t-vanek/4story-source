#pragma once

// Centralized SQL query catalog.
//
// Every SOCI-backed service reads its queries from here instead of
// inlining string literals at the call site. The benefit is that
// changes to a column name or a WHERE clause happen in one place,
// not in 9 different Soci*Service.cpp files. The boot-time schema
// validators in db/schema_validator.cpp cross-check the columns
// listed here against the live DB so a typo here surfaces during
// startup, not under load.
//
// Naming convention: one identifier per logical query, grouped by
// table. SELECT-by-id queries are named `<Table>By<Key>`; full-table
// chart loads at boot are named `All<Table>`. Insert/update
// statements land here as the consolidation pass adds them.

namespace tmapsvr::queries {

// TCURRENTUSER — session lookup for the F4 handshake handler.
inline constexpr const char* SessionByUserKey =
    "SELECT dwUserID, dwKEY, bGroupID, bChannel, szLoginIP, bLocked "
    "FROM TCURRENTUSER WHERE dwUserID = :uid AND dwKEY = :key";

// TCHARTABLE — F8 player snapshot (40 columns matching CharSnapshot).
inline constexpr const char* CharByCharId =
    "SELECT dwCharID, szNAME, bStartAct, bRealSex, bClass, bLevel, "
    "  bRace, bCountry, bOriCountry, bSex, bHair, bFace, bBody, "
    "  bPants, bHand, bFoot, bHelmetHide, dwGold, dwSilver, dwCooper, "
    "  dwEXP, dwHP, dwMP, wSkillPoint, dwRegion, bGuildLeave, "
    "  dwGuildLeaveTime, wMapID, wSpawnID, wLastSpawnID, "
    "  dwLastDestination, wTemptedMon, bAftermath, fPosX, fPosY, "
    "  fPosZ, wDIR, bStatLevel, bStatPoint, dwStatExp "
    "FROM TCHARTABLE WHERE dwCharID = :cid AND bDelete = 0";

// TINVENTABLE — F9 inventory rows per char.
inline constexpr const char* InventoryByCharId =
    "SELECT bInvenID, wItemID, dEndTime, bELD "
    "FROM TINVENTABLE WHERE dwCharID = :cid";

// TNPCCHART — F10 chart loader at boot. Whole table.
inline constexpr const char* AllNpcs =
    "SELECT wID, szName, bType, bCountryID, wLocalID, bCondition, "
    "  bDiscountRate, bAddProb, wItemID, wMapID, fPosX, fPosY, fPosZ "
    "FROM TNPCCHART";

// TSKILLTABLE — F11 per-char learned skills.
inline constexpr const char* SkillsByCharId =
    "SELECT wSkillID, bLevel, dwRemainTick "
    "FROM TSKILLTABLE WHERE dwCharID = :cid";

// TQUESTTABLE — F12 per-char accepted quests.
inline constexpr const char* QuestsByCharId =
    "SELECT dwQuestID, dwTick, bCompleteCount, bTriggerCount "
    "FROM TQUESTTABLE WHERE dwCharID = :cid";

// TQUESTTERMTABLE — F12 per-char per-quest term-progress rows.
inline constexpr const char* QuestTermsByCharId =
    "SELECT dwQuestID, dwTermID, bTermType, bCount "
    "FROM TQUESTTERMTABLE WHERE dwCharID = :cid";

// TMONSTERCHART — F13 monster template chart loaded at boot.
inline constexpr const char* AllMonsters =
    "SELECT wID, szName, bRace, bClass, wKind, bLevel, bAIType, "
    "  bRange, wChaseRange, bRoamProb, bMoneyProb, dwMinMoney, "
    "  dwMaxMoney, bItemProb, bDropCount, wExp, bIsSelf, "
    "  bRecallType, bCanSelect "
    "FROM TMONSTERCHART";

// TMONSPAWNCHART — F13 spawn-point chart loaded at boot.
inline constexpr const char* AllSpawns =
    "SELECT wID, wGroup, wLocalID, wMapID, fPosX, fPosY, fPosZ, "
    "  wDir, bCountry, bCount, bRange, bArea, bLink, bProb, bRoamType "
    "FROM TMONSPAWNCHART";

// TMAPMONCHART — spawn-point → monster linkage loaded at boot. Each
// spawn point (wSpawnID) lists its candidate monsters (wMonID) with the
// essential / leader / probability flags the SpawnManager realises from.
inline constexpr const char* AllMapMon =
    "SELECT wSpawnID, wMonID, bEssential, bLeader, bProb "
    "FROM TMAPMONCHART";

// TMONATTRCHART — per-(monster, level) combat stats loaded at boot.
// dwMaxHP is the spawn HP; wAP/wMAP/wDP/wMDP/wMinWAP/wMaxWAP feed the
// damage formula the combat layer adds.
inline constexpr const char* AllMonAttr =
    "SELECT wID, bLevel, dwMaxHP, dwMaxMP, wAP, wMAP, wDP, wMDP, "
    "  wMinWAP, wMaxWAP, dwAtkSpeed "
    "FROM TMONATTRCHART";

// TSKILLCHART — skill templates. This slice loads only the reuse
// cooldown (dwReuseDelay) the cooldown gate needs; the MP/HP cost and
// effect (TSKILLDATA) columns land with later skill waves.
inline constexpr const char* AllSkillReuse =
    "SELECT wID, dwReuseDelay FROM TSKILLCHART";

// TCOMPANIONTABLE — F15 per-char companion roster.
inline constexpr const char* CompanionsByCharId =
    "SELECT bSlot, dwMonID, bLevel, strName, dwExp, wLife, "
    "  bStatusPoints, bEffect, wSTR, wDEX, wCON, wINT, wWIS, "
    "  wMEN, wBonusID "
    "FROM TCOMPANIONTABLE WHERE dwCharID = :cid";

// TCHARTABLE — save all gameplay-mutable fields back on disconnect.
// Appearance fields (szNAME, bHair, bFace, …) are included so a single
// UPDATE covers salon-style handlers when they land; writing the same
// value back is a no-op from the DB's perspective.
// bDelete is intentionally excluded — we never want to un-delete via
// a normal save path.
inline constexpr const char* SaveCharTchart =
    "UPDATE TCHARTABLE SET "
    "  szNAME           = :name,"
    "  bHair            = :hair,   bFace    = :face,"
    "  bBody            = :body,   bPants   = :pants,"
    "  bHand            = :hand,   bFoot    = :foot,"
    "  bHelmetHide      = :helmet_hide,"
    "  bSex             = :sex,    bRealSex = :real_sex,"
    "  bCountry         = :country, bOriCountry = :ori_country,"
    "  bClass           = :class_,  bRace   = :race,"
    "  bLevel           = :level,  dwEXP    = :exp,"
    "  dwHP             = :hp,     dwMP     = :mp,"
    "  dwGold           = :gold,   dwSilver = :silver, dwCooper = :cooper,"
    "  wSkillPoint      = :sp,"
    "  dwRegion         = :region,"
    "  bGuildLeave      = :guild_leave, dwGuildLeaveTime = :guild_leave_time,"
    "  wMapID           = :map,    wSpawnID = :spawn,"
    "  wLastSpawnID     = :last_spawn,"
    "  dwLastDestination= :last_dest,"
    "  wTemptedMon      = :tempted_mon,"
    "  bAftermath       = :aftermath,"
    "  bStartAct        = :start_act,"
    "  fPosX            = :px,     fPosY    = :py, fPosZ = :pz,"
    "  wDIR             = :dir,"
    "  bStatLevel       = :stat_level, bStatPoint = :stat_point,"
    "  dwStatExp        = :stat_exp "
    "WHERE dwCharID = :cid AND bDelete = 0";

// TALLCHARTABLE — sync level/exp and compute playtime via DB-side
// DATEDIFF so we don't need to track login time in C++.
// dLoginDate was stamped by TEnterServer (or the equivalent) when the
// char entered; DATEDIFF(second, dLoginDate, GETDATE()) gives the
// elapsed session seconds.
inline constexpr const char* SaveCharTallchart =
    "UPDATE TALLCHARTABLE SET "
    "  bLevel       = :level,"
    "  dwEXP        = :exp,"
    "  dLogoutDate  = GETDATE(),"
    "  dwPlayTime   = dwPlayTime + DATEDIFF(second, dLoginDate, GETDATE()) "
    "WHERE dwCharID = :cid";

} // namespace tmapsvr::queries
