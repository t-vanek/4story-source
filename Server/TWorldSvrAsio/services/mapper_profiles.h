#pragma once

// MapperProfile bundle for TWorldSvrAsio.
//
// Bridges the guild DB-row entities (guild_entities.h) to the in-memory
// domain/registry types (guild_registry.h) via the fourstory::mapper
// Automapper, so SociGuildRepository never copies fields by hand:
//
//   GuildRow        → TGuild        (Adapt-to: TGuild holds a std::mutex,
//                                     so it is populated in place, never
//                                     constructed-by-value)
//   GuildMemberRow  → TGuildMember  (Adapt: plain copyable value type)
//
// Register + apply once at startup, mirroring TControlSvrAsio:
//
//   auto& reg = fourstory::mapper::MapperRegistry::Get();
//   reg.Register<tworldsvr::GuildMappingProfile>();
//   reg.ApplyAll();

#include "services/guild_entities.h"
#include "services/guild_registry.h"
#include "services/friend_entities.h"
#include "services/friend_repository.h"

#include "fourstory/mapper/mapper.h"

namespace tworldsvr {

class GuildMappingProfile : public fourstory::mapper::MapperProfile
{
public:
    const char* Name() const override { return "GuildMappingProfile"; }

    void Configure() override
    {
        using namespace fourstory::mapper;

        // TGUILDTABLE row → live TGuild. One Set() per persisted scalar;
        // the runtime-only fields (chief_name, country, alliance/enemy
        // lists, point_log, tactics/cabinet) are populated by other code
        // paths and intentionally left untouched here.
        MapperConfig<GuildRow, TGuild>()
            .Set(&TGuild::id,                &GuildRow::id)
            .Set(&TGuild::name,              &GuildRow::name)
            .Set(&TGuild::chief_char_id,     &GuildRow::chief)
            .Set(&TGuild::level,             &GuildRow::level)
            .Set(&TGuild::fame,              &GuildRow::fame)
            .Set(&TGuild::fame_color,        &GuildRow::fame_color)
            .Set(&TGuild::max_cabinet,       &GuildRow::max_cabinet)
            .Set(&TGuild::gold,              &GuildRow::gold)
            .Set(&TGuild::silver,            &GuildRow::silver)
            .Set(&TGuild::cooper,            &GuildRow::cooper)
            .Set(&TGuild::gi,                &GuildRow::gi)
            .Set(&TGuild::exp,               &GuildRow::exp)
            .Set(&TGuild::guild_points,      &GuildRow::guild_points)
            .Set(&TGuild::status,            &GuildRow::status)
            .Set(&TGuild::disorg,            &GuildRow::disorg)
            .Set(&TGuild::disorg_time,       &GuildRow::disorg_time)
            .Set(&TGuild::establish_time,    &GuildRow::establish_time)
            .Set(&TGuild::pvp_total_point,   &GuildRow::pvp_total_point)
            .Set(&TGuild::pvp_useable_point, &GuildRow::pvp_useable_point);

        // TGUILDMEMBERTABLE row → TGuildMember. Runtime cache fields
        // (name/level/class/tactics/castle…) stay default until
        // SetMemberConnection fills them from the live TChar.
        MapperConfig<GuildMemberRow, TGuildMember>()
            .Set(&TGuildMember::char_id,  &GuildMemberRow::char_id)
            .Set(&TGuildMember::guild_id, &GuildMemberRow::guild_id)
            .Set(&TGuildMember::duty,     &GuildMemberRow::duty)
            .Set(&TGuildMember::peer,     &GuildMemberRow::peer)
            .Set(&TGuildMember::service,  &GuildMemberRow::service);
    }
};

class FriendMappingProfile : public fourstory::mapper::MapperProfile
{
public:
    const char* Name() const override { return "FriendMappingProfile"; }

    void Configure() override
    {
        using namespace fourstory::mapper;

        // Forward edge → FriendRow (full display fields).
        MapperConfig<FriendForwardRow, FriendRow>()
            .Set(&FriendRow::id,    &FriendForwardRow::friend_id)
            .Set(&FriendRow::name,  &FriendForwardRow::name)
            .Set(&FriendRow::group, &FriendForwardRow::group)
            .Set(&FriendRow::klass, &FriendForwardRow::klass)
            .Set(&FriendRow::level, &FriendForwardRow::level);

        // Reverse edge → FriendRow (id + name only; level/class/group
        // stay default-zero, matching legacy CTBLFriendTarget).
        MapperConfig<FriendReverseRow, FriendRow>()
            .Set(&FriendRow::id,   &FriendReverseRow::char_id)
            .Set(&FriendRow::name, &FriendReverseRow::name);
    }
};

} // namespace tworldsvr
