#include "postgresql_map_service.h"
#include "native_mail_lock.h"
#include "native_wire_text.h"
#include "legacy_random.h"
#include "domain/main_transfer.h"
#include "MessageId.h"
#include "wire_codec.h"
#include <soci/soci.h>
#include <algorithm>
#include <bit>
#include <map>
#include <optional>
#include <stdexcept>
#include <vector>

namespace tmapsvr {
namespace {
std::uint16_t GameKey(std::uint8_t type,std::uint8_t wins){return type|(std::uint16_t{wins}<<8);}
std::uint8_t NpcCard(bool win,std::uint8_t card) {
    // Source SelectNpcRPS defaults to ROCK for an unknown card.
    switch(card){case 0:return win?2:1;case 1:return win?0:2;case 2:return win?1:0;default:return 0;}
}
std::string NetString(const std::string& value,std::size_t length) {
    if(length>65535)throw std::runtime_error("RPS source message length unavailable");
    constexpr char hex[]="0123456789ABCDEF";std::string prefix="00000000";
    for(unsigned i=0;i<4;++i)prefix[7-i]=hex[(length>>(4*i))&15];
    return prefix+value;
}
}
void PostgreSQLMapService::CheckNativeRpsRelease(soci::session& sql) const {
    std::string release;
    const int world=m_config.world;
    sql<<"SELECT manifest_sha256 FROM game_compat.rps_release WHERE world_id=:w AND character_manifest=:c "
        "AND actor_manifest=:a AND routing_manifest=:r",soci::use(world,"w"),
        soci::use(m_config.character_manifest,"c"),soci::use(m_config.actor_manifest,"a"),
        soci::use(m_config.routing_manifest,"r"),soci::into(release);
    if(!sql.got_data()||release!=m_rps_manifest)throw std::runtime_error("Native RPS release changed or unavailable");
}
void PostgreSQLMapService::LoadNativeRps(soci::session& sql) {
    const int world=m_config.world;long long expected=0;
    sql<<"SELECT manifest_sha256,chart_rows FROM game_compat.rps_release WHERE world_id=:w AND character_manifest=:c "
        "AND actor_manifest=:a AND routing_manifest=:r",soci::use(world,"w"),
        soci::use(m_config.character_manifest,"c"),soci::use(m_config.actor_manifest,"a"),
        soci::use(m_config.routing_manifest,"r"),soci::into(m_rps_manifest),soci::into(expected);
    if(!sql.got_data()||m_rps_manifest.size()!=64||expected<0||expected>7279)
        throw std::runtime_error("Native Map requires the full published RPS catalog");
    std::map<std::uint16_t,NativeRpsGame> games;
    {soci::rowset<soci::row> rows=(sql.prepare<<
        "SELECT g.*,CASE WHEN i1.\"wItemID\" IS NOT NULL THEN 1 ELSE 0 END AS reward1_exists,"
        "CASE WHEN i2.\"wItemID\" IS NOT NULL THEN 1 ELSE 0 END AS reward2_exists "
        "FROM game_compat.\"TRPSGAMECHART\" g "
        "LEFT JOIN character_compat.\"TITEMCHART\" i1 ON i1.\"wItemID\"=g.\"wRewardItem_1\" "
        "LEFT JOIN character_compat.\"TITEMCHART\" i2 ON i2.\"wItemID\"=g.\"wRewardItem_2\" WHERE g.world_id=:w",
        soci::use(world,"w"));
     for(const auto& row:rows) {
        const auto byte=[&](const char* name){
            if(row.get_indicator(name)!=soci::i_ok)throw std::runtime_error("NULL native RPS BYTE");
            const auto value=row.get<int>(name);if(value<0||value>255)throw std::runtime_error("Native RPS BYTE range");
            return static_cast<std::uint8_t>(value);
        };
        const auto word=[&](const char* name){
            if(row.get_indicator(name)!=soci::i_ok)throw std::runtime_error("NULL native RPS WORD");
            const auto value=row.get<int>(name);if(value < -32768||value>32767)throw std::runtime_error("Native RPS SMALLINT range");
            return std::bit_cast<std::uint16_t>(static_cast<std::int16_t>(value));
        };
        NativeRpsGame game;game.type=byte("bType");game.wins=byte("bWinCount");game.entry_item=word("wItemID");
        game.win_keep=word("wWinKeep");game.win_period=word("wWinPeriod");
        if(row.get_indicator("dwRewardMoney")!=soci::i_ok)throw std::runtime_error("NULL native RPS reward money");
        game.money=std::bit_cast<std::uint32_t>(static_cast<std::int32_t>(row.get<int>("dwRewardMoney")));
        game.probability={byte("bProb_Win"),byte("bProb_Draw"),byte("bProb_Lose")};
        // Original Map LoadData ignores zero-ID, absent-template and zero-count
        // rewards. A missing template never becomes an item with zero stats.
        const auto item1=word("wRewardItem_1"),item2=word("wRewardItem_2");
        const auto count1=byte("bItemCount_1"),count2=byte("bItemCount_2");
        if(item1&&count1&&row.get<int>("reward1_exists")){game.reward_item[0]=item1;game.reward_count[0]=count1;}
        if(item2&&count2&&row.get<int>("reward2_exists")){game.reward_item[1]=item2;game.reward_count[1]=count2;}
        if(!games.emplace(GameKey(game.type,game.wins),game).second)throw std::runtime_error("Duplicate native Map RPS key");
     }}
    if(games.size()!=static_cast<std::size_t>(expected))throw std::runtime_error("Incomplete native Map RPS catalog");
    m_rps=std::move(games);
    int probe=0;
    sql<<"SELECT count(*) FROM (SELECT before_rps,after_rps,rps_context FROM app_world.quest_operations WHERE false) c",soci::into(probe);
    sql<<"SELECT count(post_id) FROM app_world.\"TPOSTERRORTABLE\" WHERE false",soci::into(probe);
}
std::optional<NativeRpsGame> PostgreSQLMapService::FindNativeRps(std::uint8_t type,std::uint8_t wins) const {
    std::lock_guard guard(m_rps_mutex);const auto it=m_rps.find(GameKey(type,wins));
    if(it==m_rps.end())return {};
    return it->second;
}
void PostgreSQLMapService::UpdateRpsChart(std::span<const std::byte> body) {
    wire::Reader reader(body.data(),body.size());std::uint8_t group=0;std::uint16_t count=0;
    if(!reader.Read(group)||!reader.Read(count)||body.size()>65518||body.size()!=3U+9U*count)
        throw std::runtime_error("Invalid source RPS change body");
    std::vector<NativeRpsGame> edits;edits.reserve(count);
    for(unsigned i=0;i<count;++i) {
        NativeRpsGame game;
        if(!reader.Read(game.type)||!reader.Read(game.wins)||!reader.Read(game.probability[0])||
           !reader.Read(game.probability[1])||!reader.Read(game.probability[2])||!reader.Read(game.win_keep)||!reader.Read(game.win_period))
            throw std::runtime_error("Truncated source RPS change");
        edits.push_back(game);
    }
    if(!reader.Eof())throw std::runtime_error("Trailing source RPS change");
    std::lock_guard guard(m_rps_mutex);
    for(const auto& edit:edits)if(const auto it=m_rps.find(GameKey(edit.type,edit.wins));it!=m_rps.end()) {
        it->second.probability=edit.probability;it->second.win_keep=edit.win_keep;it->second.win_period=edit.win_period;
    }
    // Source bGroup is forwarded but not consulted. Overrides are not saved.
}
void PostgreSQLMapService::RewardNativeRps(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,const NativeRpsGame& game) {
    struct Text {std::string decoded,bytes;};std::map<int,Text> messages;
    {soci::rowset<soci::row> rows=(sql.prepare<<
        "SELECT \"dwID\",\"szMessage\",encode(message_bytes,'hex') FROM game_compat.server_messages WHERE \"dwID\" IN (6,45,46)");
     for(const auto& row:rows) {
        if(row.get_indicator(1)!=soci::i_ok||row.get_indicator(2)!=soci::i_ok)
            throw std::runtime_error("RPS message source bytes unavailable");
        if(!messages.emplace(row.get<int>(0),Text{row.get<std::string>(1),native_wire_text::Unhex(row.get<std::string>(2))}).second)
            throw std::runtime_error("Duplicate RPS message ID");
     }}
    // GetSvrMsg's actual absent-row branch is NAME_NULL. Publication proves the
    // complete chart, so this is distinct from missing/unloaded source data.
    const auto message=[&](int id){const auto it=messages.find(id);return it==messages.end()?Text{}:it->second;};
    const auto sender=message(6),title=message(45),text=message(46);
    const auto title_bytes=NetString(title.bytes,title.bytes.size()),message_bytes=NetString(text.bytes,text.bytes.size());
    if(sender.bytes.size()>50||title_bytes.size()>256||message_bytes.size()>2048||
       sender.bytes.find('\0')!=std::string::npos||title_bytes.find('\0')!=std::string::npos||message_bytes.find('\0')!=std::string::npos)
        throw std::runtime_error("RPS source mail buffer limit");
    const auto title_decoded=NetString(title.decoded,title.bytes.size()),message_decoded=NetString(text.decoded,text.bytes.size());
    const auto sender_hex=native_wire_text::Hex(sender.bytes),title_hex=native_wire_text::Hex(title_bytes),message_hex=native_wire_text::Hex(message_bytes);
    std::vector<transfer::Item> rewards;
    for(unsigned i=0;i<2;++i)if(game.reward_item[i]) {
        auto item=CreateQuestItem(sql,c,game.reward_item[i],game.reward_count[i],s.payload->event_time);
        if(!item||!item->source)throw std::runtime_error("Pinned RPS reward template disappeared");
        rewards.push_back(*item->source);
    }
    // SendDM_POSTRECVFROMSVR_REQ pops the vector from its back. Preserve source
    // attachment traversal; private SQL slots only disambiguate native storage.
    std::reverse(rewards.begin(),rewards.end());
    LockNativeMailbox(sql,c.group,c.char_id);
    const int world=c.group,character=std::bit_cast<std::int32_t>(c.char_id);
    const int gold=game.money/1000000U,silver=(game.money/1000U)%1000U,cooper=game.money%1000U;
    const int contains=!rewards.empty()||game.money?1:0;const long long now=s.payload->event_time,stored=((now+30)/60)*60;
    int calendar_valid=0;
    sql<<"SELECT CASE WHEN (to_timestamp(:time) AT TIME ZONE 'UTC') BETWEEN timestamp '1900-01-01' "
        "AND timestamp '2079-06-06 23:59:00' THEN 1 ELSE 0 END",soci::use(stored,"time"),soci::into(calendar_valid);
    if(!calendar_valid)throw std::runtime_error("RPS post exceeds source SMALLDATETIME range");
    int post=0;
    const auto recipient=native_wire_text::Decode(sql,s.szNAME,m_config.client_text_encoding);
    const auto recipient_hex=native_wire_text::Hex(s.szNAME);
    sql<<"INSERT INTO app_world.\"TPOSTTABLE\"(\"bWorldID\",\"dwCharID\",\"szRecvName\",\"bType\",\"bRead\",\"timeRecv\","
        "\"dwSendID\",\"szSender\",\"szTitle\",\"szMessage\",\"dwGold\",\"dwSilver\",\"dwCooper\",\"bContain\",sender_bytes,recipient_bytes,title_bytes,message_bytes) "
        "VALUES(:w,:c,:name,1,0,to_timestamp(:time) AT TIME ZONE 'UTC',0,:sender,:title,:message,:g,:s,:co,:contains,"
        "decode(:sb,'hex'),decode(:rb,'hex'),decode(:tb,'hex'),decode(:mb,'hex')) RETURNING \"dwPostID\"",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(recipient,"name"),soci::use(stored,"time"),
        soci::use(sender.decoded,"sender"),soci::use(title_decoded,"title"),soci::use(message_decoded,"message"),
        soci::use(gold,"g"),soci::use(silver,"s"),soci::use(cooper,"co"),soci::use(contains,"contains"),
        soci::use(sender_hex,"sb"),soci::use(recipient_hex,"rb"),soci::use(title_hex,"tb"),soci::use(message_hex,"mb"),soci::into(post);
    if(!sql.got_data()||post<=0)throw std::runtime_error("RPS post allocation failed");
    for(std::size_t i=0;i<rewards.size();++i) {
        auto raw=rewards[i];raw.storage=2;raw.storage_id=static_cast<std::uint32_t>(post);raw.slot=static_cast<std::uint8_t>(i);
        // TSaveItemDirect has SMALLDATETIME expiry. Preserve original minute
        // rounding before raw SQL and graph hydration share this new ownership.
        if(raw.expires) {
            raw.expires=((raw.expires+30)/60)*60;
            const long long expires=raw.expires;int valid=0;
            sql<<"SELECT CASE WHEN (to_timestamp(:time) AT TIME ZONE 'UTC') BETWEEN timestamp '1900-01-01' "
                "AND timestamp '2079-06-06 23:59:00' THEN 1 ELSE 0 END",soci::use(expires,"time"),soci::into(valid);
            if(!valid)throw std::runtime_error("RPS attachment expiry exceeds source SMALLDATETIME range");
        }
        InsertNativeItem(sql,c,raw);
    }
    if(gold>10)sql<<"INSERT INTO app_world.\"TPOSTERRORTABLE\"(\"bWorldID\",\"dwCharID\",\"dwGold\",post_id) VALUES(:w,:c,:g,:p)",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(gold,"g"),soci::use(post,"p");
    sql<<"INSERT INTO app_world.mail_notifications(world_id,char_id,post_id,post_type,enqueue_second) VALUES(:w,:c,:p,1,:now)",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(post,"p"),soci::use(now,"now");
    auto payload=std::make_shared<CharacterPayload>(*s.payload);s.payload=payload;
    HydrateNativePosts(sql,c,*payload); // refresh actual attachments, including a complete graph
}
bool PostgreSQLMapService::ChangeNativeRps(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,std::uint16_t opcode,
    std::span<const std::byte> body,const QuestMapFacts& facts,QuestPackets& packets,std::string& context) {
    using tnetlib::protocol::MessageId;
    if(!facts.ordinary_rps)throw std::runtime_error("RPS mode is not implemented");
    CheckNativeRpsRelease(sql);const auto before=s.rps;
    if(s.rps.pending&&(s.rps.epoch!=c.authority_epoch||s.rps.connection!=c.connection_id))s.rps.ClearPending();
    context="{\"release\":\""+m_rps_manifest+"\"}";
    wire::Reader reader(body.data(),body.size());const auto message=static_cast<MessageId>(opcode);
    const auto start_reply=[&](std::uint8_t result) {
        std::vector<std::byte> ack;wire::WritePOD(ack,result);wire::WritePOD(ack,s.dwGold);
        wire::WritePOD(ack,s.dwSilver);wire::WritePOD(ack,s.dwCooper);
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_RPSSTART_ACK),std::move(ack));
    };
    if(message==MessageId::CS_RPSSTART_REQ) {
        std::uint8_t type=0,inventory=0,slot=0;
        if(!reader.Read(type)||!reader.Read(inventory)||!reader.Read(slot)||!reader.Eof())throw std::runtime_error("Invalid RPSSTART body");
        if(s.rps.type){start_reply(1);return s.rps!=before;}
        const auto game=FindNativeRps(type,0);
        if(!game){start_reply(2);return s.rps!=before;}
        context="{\"release\":\""+m_rps_manifest+"\",\"game\":"+game->Json()+'}';
        const auto bag=std::find_if(s.payload->bags.begin(),s.payload->bags.end(),[&](const auto& b){return b.bag.bInvenID==inventory;});
        if(bag==s.payload->bags.end()){start_reply(3);return s.rps!=before;}
        const auto item=std::find_if(bag->items.begin(),bag->items.end(),[&](const auto& i){return i.bItemID==slot;});
        if(item==bag->items.end()||item->wItemID!=game->entry_item||!item->bCount){start_reply(3);return s.rps!=before;}
        DebitNativeItem(sql,c,s,inventory,slot,item->dlID,1,packets);
        if(inventory==254)RefreshEquipment(sql,s,true); // cached getters, no invented equipment ACK
        s.rps={};s.rps.type=type;start_reply(0);return true;
    }
    if(!s.rps.type)return s.rps!=before;
    std::uint8_t card=0;bool win=false;
    if(message==MessageId::MW_RPSGAME_REQ) {
        std::uint32_t who=0,key=0;std::uint8_t result=0;
        if(!reader.Read(who)||!reader.Read(key)||!reader.Read(result)||!reader.Read(card)||!reader.Eof()||who!=c.char_id||key!=c.key||result>1)
            throw std::runtime_error("Invalid World RPS result");
        if(!s.rps.pending||s.rps.card!=card)return s.rps!=before;
        s.rps.ClearPending();win=result!=0;
    } else {
        if(message!=MessageId::CS_RPSGAME_REQ||!reader.Read(card)||!reader.Eof())throw std::runtime_error("Invalid RPSGAME body");
        if(s.rps.pending)return s.rps!=before;
        const auto game=FindNativeRps(s.rps.type,s.rps.wins);
        if(!game){s.rps={};return true;}
        context="{\"release\":\""+m_rps_manifest+"\",\"game\":"+game->Json();
        if(card>=3) {
            context+=",\"giveup\":true}";
            if(s.rps.wins)RewardNativeRps(sql,c,s,*game);
            s.rps={};return true;
        }
        std::optional<std::uint32_t> draw;
        if(game->probability[2])draw=LegacyRandom15();
        const auto rolled=draw?*draw%game->probability[2]:255U;
        context+=",\"draw\":"+(draw?std::to_string(*draw):std::string{"null"})+",\"roll\":"+std::to_string(rolled)+'}';
        win=rolled<game->probability[0];
        if(win&&game->win_keep) {
            if(!facts.world_registered)throw std::runtime_error("RPS win limit requires registered World");
            s.rps.pending=true;s.rps.card=card;s.rps.epoch=c.authority_epoch;s.rps.connection=c.connection_id;
            std::vector<std::byte> request;wire::WritePOD(request,c.char_id);wire::WritePOD(request,c.key);
            wire::WritePOD(request,s.rps.type);wire::WritePOD(request,static_cast<std::uint8_t>(s.rps.wins+1U));wire::WritePOD(request,card);
            packets.emplace_back(static_cast<std::uint16_t>(MessageId::MW_RPSGAME_ACK),std::move(request),QuestAudience::World);
            return true;
        }
        if(!win&&rolled<game->probability[1]) {
            std::vector<std::byte> ack{static_cast<std::byte>(s.rps.wins),static_cast<std::byte>(card),static_cast<std::byte>(card)};
            packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_RPSGAME_ACK),std::move(ack));
            if(!FindNativeRps(s.rps.type,static_cast<std::uint8_t>(s.rps.wins+1U))) {
                if(s.rps.wins)RewardNativeRps(sql,c,s,*game);
                s.rps={};
            }
            return true; // retain the actual random observation and one-use local event
        }
    }
    if(win)s.rps.wins=static_cast<std::uint8_t>(s.rps.wins+1U);else s.rps={};
    packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_RPSGAME_ACK),
        std::vector<std::byte>{static_cast<std::byte>(s.rps.wins),static_cast<std::byte>(card),static_cast<std::byte>(NpcCard(win,card))});
    if(!FindNativeRps(s.rps.type,static_cast<std::uint8_t>(s.rps.wins+1U))) {
        if(s.rps.wins)if(const auto game=FindNativeRps(s.rps.type,s.rps.wins))RewardNativeRps(sql,c,s,*game);
        s.rps={};
    }
    return true;
}
}
