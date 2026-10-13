#include "postgresql_map_service.h"
#include "native_wire_text.h"
#include <soci/soci.h>
#include <bit>
#include <iterator>

namespace tmapsvr {
bool PostgreSQLMapService::QueueQuestPost(soci::session& sql,const MapSessionClaim& c,const CharSnapshot& s,
    const QuestDef& def,std::uint8_t roll,const std::function<std::uint8_t()>& probability) {
    // Called only inside ChangeQuest's account/current primary/owner/head-fenced
    // transaction. Queue inserts, child effects and checkpoint commit together.
    int compatible=0;
    sql << "SELECT count(*) FROM game_compat.quest_mail_release r JOIN game_compat.catalog_release m "
        "ON m.manifest_sha256=r.map_manifest AND m.status='verified' "
        "JOIN character_compat.catalog_release c ON c.manifest_sha256=r.character_manifest AND c.status='verified' "
        "JOIN actor_compat.catalog_release a ON a.manifest_sha256=r.actor_manifest AND a.status='verified' WHERE r.status='verified'",
        soci::into(compatible);
    if(compatible!=1)throw std::runtime_error("Quest mail requires the published original title/reward catalog");
    if(def.rewards.size()>255)throw std::runtime_error("Quest mail exceeds source BYTE reward iteration");
    std::vector<transfer::Item> items;std::uint8_t sum=0;
    for(const auto& reward:def.rewards) {
        if(reward.bRewardType!=RT_ITEM&&reward.bRewardType!=RT_MAGICITEM)continue;
        bool take=false;
        switch(reward.bTakeMethod) {
        case RM_PROB:take=probability()<reward.bTakeData;break;
        case RM_RANDOM:sum=static_cast<std::uint8_t>(sum+reward.bTakeData);take=sum>roll;break;
        case RM_DEFAULT:take=true;break;
        default:break; // Source SENDPOST deliberately does not handle RM_SELECT.
        }
        if(!take)continue;
        auto created=CreateQuestRewardRawItems(sql,c,s,reward,true);
        items.insert(items.end(),std::make_move_iterator(created.begin()),std::make_move_iterator(created.end()));
    }
    if(items.empty())return false; // Original bRun remains false: no children.
    if(items.size()>255)throw std::runtime_error("Quest mail exceeds source BYTE item packet count");
    const int quest=std::bit_cast<std::int32_t>(def.dwQuestID);soci::row text;
    sql << "SELECT valid::integer AS valid,encode(sender_bytes,'hex') AS sender,encode(title_bytes,'hex') AS title,"
        "encode(message_bytes,'hex') AS message FROM game_compat.quest_mail_text WHERE \"dwQuestID\"=:q",
        soci::use(quest),soci::into(text);
    // TQuestSendPost returns2 on absent/NULL title, which SSHandler does not
    // examine. The already true bRun still executes children; it sends no ACK.
    if(!sql.got_data()||!text.get<int>("valid"))return true;
    const auto sender_hex=text.get<std::string>("sender"),title_hex=text.get<std::string>("title"),message_hex=text.get<std::string>("message");
    const auto sender_bytes=native_wire_text::Unhex(sender_hex),title_bytes=native_wire_text::Unhex(title_hex),
        message_bytes=native_wire_text::Unhex(message_hex);
    if(sender_bytes.size()>50)throw std::runtime_error("Quest sender exceeds original reserved-post column");
    // The backup column collation is explicitly Latin1 (CP1252). No client ACP
    // or Unicode conversion is guessed: preserve wire bytes independently.
    const auto sender=native_wire_text::Decode(sql,sender_bytes,"WIN1252"),
        title=native_wire_text::Decode(sql,title_bytes,"WIN1252"),message=native_wire_text::Decode(sql,message_bytes,"WIN1252");
    for(const auto& raw:items) {
        if(raw.magic.size()>6)throw std::runtime_error("Quest mail exceeds source magic capacity");
        std::string json="{";
        const auto field=[&](const std::string& name,auto value) {
            if(json.size()>1)json+=',';
            json+='"'+name+"\":"+std::to_string(value);
        };
        field("bWorldID",c.group);field("dwRecverID",std::bit_cast<std::int32_t>(c.char_id));field("bSend",0);
        field("wItemID",std::bit_cast<std::int16_t>(raw.item));field("bLevel",raw.level);field("bCount",raw.count);
        field("bGLevel",raw.grade);field("dwDuraMax",std::bit_cast<std::int32_t>(raw.durability_max));
        field("dwDuraCur",std::bit_cast<std::int32_t>(raw.durability));field("bRefineCur",raw.refine);
        // Source INSERT ignores its grade-effect parameter and has default0;
        // bGem is literal0. Preserve these losses, not the magic template's values.
        field("bGradeEffect",0);field("bGem",0);
        // Explicit repair of the backup procedure's NOT NULL/no-default omission:
        // use the actual CTItem appearance, ordinary constructor0; never a guess.
        field("wMoggItemID",std::bit_cast<std::int16_t>(raw.appearance));
        for(unsigned i=0;i<6;++i) {
            field("bMagic"+std::to_string(i+1),i<raw.magic.size()?raw.magic[i].id:0);
            field("wValue"+std::to_string(i+1),i<raw.magic.size()?std::bit_cast<std::int16_t>(raw.magic[i].value):0);
        }
        const std::uint32_t ext[]={raw.eld,raw.wrap,raw.color,raw.guild,raw.companion,raw.texture};
        for(unsigned i=0;i<6;++i)field("dwTime"+std::to_string(i+1),std::bit_cast<std::int32_t>(ext[i]));
        json+='}';const long long expires=raw.expires;long long queued=0;
        // __TIMETODB(0) uses1900-01-01. Other expirations bind SMALLDATETIME,
        // including minute rounding and its representable calendar range.
        sql << "WITH v AS (SELECT (jsonb_populate_record(NULL::app_world.\"TRESERVEDPOST\",CAST(:item AS jsonb)||"
            "jsonb_build_object('szSender',CAST(:sender AS text),'szTitle',CAST(:title AS text),'szMessage',CAST(:message AS text),"
            "'dEndTime',CASE WHEN :expires=0 THEN TIMESTAMP '1900-01-01' ELSE date_trunc('minute',to_timestamp(:expires+30) AT TIME ZONE 'UTC') END))).*) "
            "INSERT INTO app_world.\"TRESERVEDPOST\"(\"bWorldID\",\"dwRecverID\",\"szSender\",\"szTitle\",\"szMessage\",\"bSend\","
            "\"wItemID\",\"bLevel\",\"bCount\",\"bGLevel\",\"dwDuraMax\",\"dwDuraCur\",\"bRefineCur\",\"dEndTime\",\"bGradeEffect\","
            "\"bMagic1\",\"bMagic2\",\"bMagic3\",\"bMagic4\",\"bMagic5\",\"bMagic6\",\"wValue1\",\"wValue2\",\"wValue3\",\"wValue4\",\"wValue5\",\"wValue6\","
            "\"dwTime1\",\"dwTime2\",\"dwTime3\",\"dwTime4\",\"dwTime5\",\"dwTime6\",\"bGem\",\"wMoggItemID\",sender_bytes,title_bytes,message_bytes) "
            "SELECT \"bWorldID\",\"dwRecverID\",\"szSender\",\"szTitle\",\"szMessage\",\"bSend\",\"wItemID\",\"bLevel\",\"bCount\",\"bGLevel\","
            "\"dwDuraMax\",\"dwDuraCur\",\"bRefineCur\",\"dEndTime\",\"bGradeEffect\",\"bMagic1\",\"bMagic2\",\"bMagic3\",\"bMagic4\",\"bMagic5\",\"bMagic6\","
            "\"wValue1\",\"wValue2\",\"wValue3\",\"wValue4\",\"wValue5\",\"wValue6\",\"dwTime1\",\"dwTime2\",\"dwTime3\",\"dwTime4\",\"dwTime5\",\"dwTime6\","
            "\"bGem\",\"wMoggItemID\",decode(:sender_bytes,'hex'),decode(:title_bytes,'hex'),decode(:message_bytes,'hex') FROM v "
            "WHERE \"dEndTime\" BETWEEN TIMESTAMP '1900-01-01' AND TIMESTAMP '2079-06-06 23:59' RETURNING queue_id",
            soci::use(json,"item"),soci::use(sender,"sender"),soci::use(title,"title"),soci::use(message,"message"),soci::use(expires,"expires"),
            soci::use(sender_hex,"sender_bytes"),soci::use(title_hex,"title_bytes"),soci::use(message_hex,"message_bytes"),soci::into(queued);
        if(!sql.got_data()||queued<=0)throw std::runtime_error("Quest mail expiry exceeds original SQL calendar");
    }
    return true;
}
}
