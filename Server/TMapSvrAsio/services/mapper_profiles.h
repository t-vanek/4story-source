#pragma once

// MapperProfile bundle for TMapSvrAsio.
//
// Bridges the DB-row entities (char_entities.h, …) to the domain
// snapshots (domain/…) via the fourstory::mapper Automapper. The
// numeric narrowing (DB int32 → uint8/16/32, double → float) is handled
// by fourstory::mapper::Convert, so the SOCI services drop their
// hand-written db::Narrow* copy blocks:
//
//   CharRow → CharSnapshot
//
// Register + apply once at startup (mirrors TWorldSvrAsio /
// TControlSvrAsio):
//
//   auto& reg = fourstory::mapper::MapperRegistry::Get();
//   reg.Register<tmapsvr::CharMappingProfile>();
//   reg.ApplyAll();

#include "services/char_entities.h"
#include "domain/character.h"

#include "fourstory/mapper/mapper.h"

namespace tmapsvr {

class CharMappingProfile : public fourstory::mapper::MapperProfile
{
public:
    const char* Name() const override { return "CharMappingProfile"; }

    void Configure() override
    {
        using namespace fourstory::mapper;

        // TCHARTABLE row → CharSnapshot. Convert narrows each int32 to the
        // snapshot's tinyint/smallint width and each double position to
        // float. Fields the snapshot fills from other services (items,
        // skills, quests, secure-code, …) are left untouched here.
        MapperConfig<CharRow, CharSnapshot>()
            .Set(&CharSnapshot::dwCharID,         &CharRow::char_id)
            .Set(&CharSnapshot::szNAME,           &CharRow::name)
            .Set(&CharSnapshot::bStartAct,        &CharRow::start_act)
            .Set(&CharSnapshot::bRealSex,         &CharRow::real_sex)
            .Set(&CharSnapshot::bClass,           &CharRow::klass)
            .Set(&CharSnapshot::bLevel,           &CharRow::level)
            .Set(&CharSnapshot::bRace,            &CharRow::race)
            .Set(&CharSnapshot::bCountry,         &CharRow::country)
            .Set(&CharSnapshot::bOriCountry,      &CharRow::ori_country)
            .Set(&CharSnapshot::bSex,             &CharRow::sex)
            .Set(&CharSnapshot::bHair,            &CharRow::hair)
            .Set(&CharSnapshot::bFace,            &CharRow::face)
            .Set(&CharSnapshot::bBody,            &CharRow::body)
            .Set(&CharSnapshot::bPants,           &CharRow::pants)
            .Set(&CharSnapshot::bHand,            &CharRow::hand)
            .Set(&CharSnapshot::bFoot,            &CharRow::foot)
            .Set(&CharSnapshot::bHelmetHide,      &CharRow::helmet_hide)
            .Set(&CharSnapshot::dwGold,           &CharRow::gold)
            .Set(&CharSnapshot::dwSilver,         &CharRow::silver)
            .Set(&CharSnapshot::dwCooper,         &CharRow::cooper)
            .Set(&CharSnapshot::dwEXP,            &CharRow::exp)
            .Set(&CharSnapshot::dwHP,             &CharRow::hp)
            .Set(&CharSnapshot::dwMP,             &CharRow::mp)
            .Set(&CharSnapshot::wSkillPoint,      &CharRow::skill_point)
            .Set(&CharSnapshot::dwRegion,         &CharRow::region)
            .Set(&CharSnapshot::bGuildLeave,      &CharRow::guild_leave)
            .Set(&CharSnapshot::dwGuildLeaveTime, &CharRow::guild_leave_time)
            .Set(&CharSnapshot::wMapID,           &CharRow::map)
            .Set(&CharSnapshot::wSpawnID,         &CharRow::spawn)
            .Set(&CharSnapshot::wLastSpawnID,     &CharRow::last_spawn)
            .Set(&CharSnapshot::dwLastDestination,&CharRow::last_dest)
            .Set(&CharSnapshot::wTemptedMon,      &CharRow::tempted_mon)
            .Set(&CharSnapshot::bAftermath,       &CharRow::aftermath)
            .Set(&CharSnapshot::fPosX,            &CharRow::pos_x)
            .Set(&CharSnapshot::fPosY,            &CharRow::pos_y)
            .Set(&CharSnapshot::fPosZ,            &CharRow::pos_z)
            .Set(&CharSnapshot::wDIR,             &CharRow::dir)
            .Set(&CharSnapshot::bStatLevel,       &CharRow::stat_level)
            .Set(&CharSnapshot::bStatPoint,       &CharRow::stat_point)
            .Set(&CharSnapshot::dwStatExp,        &CharRow::stat_exp);
    }
};

} // namespace tmapsvr
