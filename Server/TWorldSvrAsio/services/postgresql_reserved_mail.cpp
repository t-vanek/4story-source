#include "postgresql_reserved_mail.h"
#include "fourstory/db/native_mail_lock.h"
#include <soci/soci.h>
#include <algorithm>
#include <set>
#include <stdexcept>
#include <vector>

namespace tworldsvr {
namespace {
bool TryLock(soci::session& sql,long long key) {
    int locked=0;
    sql << "SELECT CASE WHEN pg_try_advisory_xact_lock(CAST(:key AS bigint)) THEN 1 ELSE 0 END",
        soci::use(key,"key"),soci::into(locked);
    return locked!=0;
}
void Timeouts(soci::session& sql) {
    sql << "SET LOCAL lock_timeout='5s'";
    sql << "SET LOCAL statement_timeout='15s'";
}
std::string OriginalBytes(const soci::row& row,const char* encoded,const char* decoded) {
    if(row.get_indicator(encoded)!=soci::i_null)return row.get<std::string>(encoded);
    const auto text=row.get<std::string>(decoded);std::string hex;
    constexpr char digits[]="0123456789abcdef";
    for(unsigned char b:text) {
        if(!b||b>=128)throw std::runtime_error("Offline bill requires original non-ASCII mail bytes");
        hex+=digits[b>>4];hex+=digits[b&15];
    }
    return hex;
}
}
PostgreSQLReservedMail::PostgreSQLReservedMail(fourstory::db::SessionPool& pool,std::uint8_t world)
    :m_pool(pool),m_world(world) {
    if(pool.GetBackend()!=fourstory::db::Backend::PostgreSQL)
        throw std::runtime_error("Reserved mail requires the native PostgreSQL backend");
    auto lease=m_pool.Acquire();auto& sql=*lease;soci::transaction tx(sql);Timeouts(sql);
    const int w=m_world;int exists=0;
    sql << "SELECT 1 FROM app_world.worlds WHERE group_id=:w",soci::use(w),soci::into(exists);
    if(!sql.got_data())throw std::runtime_error("Reserved mail native world is missing");
    // Fail closed on missing063/062. Do not convert SQL errors into an empty
    // queue or silently fall back to a legacy/in-memory producer.
    sql << "SELECT count(*) FROM app_world.\"TRESERVEDPOST\" WHERE false",soci::into(exists);
    sql << "SELECT count(last_minute) FROM app_world.reserved_mail_clock WHERE false",soci::into(exists);
    sql << "SELECT count(enqueue_second) FROM app_world.mail_notifications WHERE false",soci::into(exists);
    sql << "SELECT count(last_minute) FROM app_world.offline_bill_clock WHERE false",soci::into(exists);
    sql << "SELECT count(post_id) FROM app_world.offline_bill_returns WHERE false",soci::into(exists);
    tx.commit();
}
ReservedMailBatch PostgreSQLReservedMail::Deliver(std::int64_t now) {
    std::lock_guard guard(m_mutex);
    if(m_commit_unknown)throw std::runtime_error("Reserved mail stopped after an unknown COMMIT result");
    if(now<0)throw std::runtime_error("Reserved mail source clock is invalid");
    auto lease=m_pool.Acquire();auto& sql=*lease;soci::transaction tx(sql);Timeouts(sql);
    ReservedMailBatch result;const int world=m_world;
    if(!TryLock(sql,fourstory::db::ReservedMailBatchKey(m_world))){result.busy=true;return result;}
    const long long minute=now/60;int claimed=0;
    sql << "INSERT INTO app_world.reserved_mail_clock(world_id,last_minute) VALUES(:w,:minute) "
        "ON CONFLICT(world_id) DO UPDATE SET last_minute=EXCLUDED.last_minute,updated_at=clock_timestamp() "
        "WHERE app_world.reserved_mail_clock.last_minute<>EXCLUDED.last_minute RETURNING world_id",
        soci::use(world,"w"),soci::use(minute,"minute"),soci::into(claimed);
    if(!sql.got_data()){result.minute_already_processed=true;return result;}
    struct Pending {long long id;int recipient;};std::vector<Pending> pending;
    {
        soci::rowset<soci::row> rows=(sql.prepare <<
            "SELECT queue_id,\"dwRecverID\" FROM app_world.\"TRESERVEDPOST\" "
            "WHERE \"bWorldID\"=:w AND \"bSend\"=0 ORDER BY queue_id LIMIT 30 FOR UPDATE",
            soci::use(world,"w"));
        for(const auto& row:rows)pending.push_back({row.get<long long>(0),row.get<int>(1)});
    }
    // Acquire every recipient mailbox before the shared item allocator. Never
    // wait on a mailbox held by a Map transaction that might await that index.
    // A busy batch rolls its minute claim back; retry has a known rollback.
    std::set<std::uint32_t> recipients;
    for(const auto& q:pending)recipients.insert(static_cast<std::uint32_t>(q.recipient));
    for(const auto recipient:recipients)if(!TryLock(sql,fourstory::db::NativeMailboxKey(m_world,recipient))) {
        result.busy=true;return result;
    }
    const long long second=now,stored_second=((now+30)/60)*60;
    for(const auto& q:pending) {
        std::string recipient_name;
        // Source TGetReservedPost includes soft-deleted rows; actual absence
        // settles this row as2 and breaks, leaving the rest for the next minute.
        sql << "SELECT \"szNAME\" FROM app_world.\"TCHARTABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c FOR KEY SHARE",
            soci::use(world,"w"),soci::use(q.recipient,"c"),soci::into(recipient_name);
        if(!sql.got_data()) {
            sql << "UPDATE app_world.\"TRESERVEDPOST\" SET \"bSend\"=2,settled_at=clock_timestamp() WHERE queue_id=:id AND \"bSend\"=0",
                soci::use(q.id,"id");
            result.missing=1;break;
        }
        if(!std::all_of(recipient_name.begin(),recipient_name.end(),[](unsigned char b){return b>0&&b<128;}))
            throw std::runtime_error("Reserved mail non-ASCII native recipient name requires its original wire projection");
        int malformed=0;
        sql << "SELECT CASE WHEN \"bCount\"=0 OR \"wItemID\"=0 OR "
            "position(decode('00','hex') IN sender_bytes)>0 OR position(decode('00','hex') IN title_bytes)>0 OR "
            "position(decode('00','hex') IN message_bytes)>0 THEN 1 ELSE 0 END "
            "FROM app_world.\"TRESERVEDPOST\" WHERE queue_id=:id",soci::use(q.id,"id"),soci::into(malformed);
        if(malformed)throw std::runtime_error("Reserved mail source row is not representable in native item/wire state");
        int post=0;
        sql << "INSERT INTO app_world.\"TPOSTTABLE\"(\"bWorldID\",\"dwCharID\",\"szRecvName\",\"bType\",\"bRead\",\"timeRecv\","
            "\"dwSendID\",\"szSender\",\"szTitle\",\"szMessage\",\"dwGold\",\"dwSilver\",\"dwCooper\","
            "sender_bytes,recipient_bytes,title_bytes,message_bytes) "
            "SELECT :w,q.\"dwRecverID\",:name,1,0,to_timestamp(:time) AT TIME ZONE 'UTC',COALESCE((SELECT c.\"dwCharID\" "
            "FROM app_world.\"TCHARTABLE\" c WHERE c.\"bWorldID\"=:w AND lower(rtrim(c.\"szNAME\") COLLATE \"C\")="
            "lower(rtrim(q.\"szSender\") COLLATE \"C\")),0),q.\"szSender\",q.\"szTitle\",q.\"szMessage\",0,0,0,"
            "q.sender_bytes,convert_to(:name,'UTF8'),q.title_bytes,q.message_bytes FROM app_world.\"TRESERVEDPOST\" q "
            "WHERE q.queue_id=:id AND q.\"bSend\"=0 RETURNING \"dwPostID\"",
            soci::use(world,"w"),soci::use(recipient_name,"name"),soci::use(stored_second,"time"),soci::use(q.id,"id"),soci::into(post);
        if(!sql.got_data()||post<=0)throw std::runtime_error("Reserved mail post was not allocated");
        long long item=0;
        sql << "UPDATE app_world.worlds SET item_high_water=item_high_water+1 WHERE group_id=:w "
            "AND item_high_water::numeric+1 < (item_world::numeric+1)*72057594037927936 RETURNING item_high_water",
            soci::use(world,"w"),soci::into(item);
        if(!sql.got_data()||item<=0)throw std::runtime_error("Reserved mail item identity range exhausted");
        // TSaveItemDirect preserves the six raw percentages/extensions. Never
        // regenerate attributes from a template, move an old item identity, or
        // delete a collided global item as the legacy procedure did.
        sql << "INSERT INTO app_world.\"TITEMTABLE\" SELECT (jsonb_populate_record(NULL::app_world.\"TITEMTABLE\","
            "to_jsonb(q)||jsonb_build_object('dlID',CAST(:item AS bigint),'bStorageType',2,'dwStorageID',CAST(:post AS integer),"
            "'bOwnerType',0,'dwOwnerID',q.\"dwRecverID\",'bItemID',0,'post_id',CAST(:post AS integer),"
            "'wCustomTex',q.\"dwTime6\" & 65535))).* FROM app_world.\"TRESERVEDPOST\" q WHERE queue_id=:id AND \"bSend\"=0",
            soci::use(item,"item"),soci::use(post,"post"),soci::use(q.id,"id");
        const int type=1;
        sql << "INSERT INTO app_world.mail_notifications(world_id,char_id,post_id,post_type,enqueue_second) VALUES(:w,:c,:p,:type,:time)",
            soci::use(world,"w"),soci::use(q.recipient,"c"),soci::use(post,"p"),soci::use(type,"type"),soci::use(second,"time");
        long long settled=0;
        sql << "UPDATE app_world.\"TRESERVEDPOST\" SET \"bSend\"=1,delivered_post_id=:post,delivered_item_id=:item,"
            "settled_at=clock_timestamp() WHERE queue_id=:id AND \"bSend\"=0 RETURNING queue_id",
            soci::use(post,"post"),soci::use(item,"item"),soci::use(q.id,"id"),soci::into(settled);
        if(!sql.got_data()||settled!=q.id)throw std::runtime_error("Reserved mail queue ownership changed");
        ++result.delivered;
    }
    // No automatic write replay after an uncertain COMMIT. The scheduler
    // treats this as fatal; restart still reads durable queue status/lineage.
    try {tx.commit();}catch(...){m_commit_unknown=true;throw;}
    return result;
}
OfflineBillBatch PostgreSQLReservedMail::ReturnOfflineBills(std::int64_t now) {
    std::lock_guard guard(m_mutex);
    if(m_commit_unknown)throw std::runtime_error("Mail stopped after an unknown COMMIT result");
    if(now<0)throw std::runtime_error("Offline bill source clock is invalid");
    auto lease=m_pool.Acquire();auto& sql=*lease;soci::transaction tx(sql);Timeouts(sql);
    OfflineBillBatch result;const int world=m_world;
    if(!TryLock(sql,fourstory::db::OfflineBillBatchKey(m_world))){result.busy=true;return result;}
    const long long minute=now/60,second=now,stored_second=((now+30)/60)*60;int claimed=0;
    sql << "INSERT INTO app_world.offline_bill_clock(world_id,last_minute) VALUES(:w,:minute) "
        "ON CONFLICT(world_id) DO UPDATE SET last_minute=EXCLUDED.last_minute,updated_at=clock_timestamp() "
        "WHERE app_world.offline_bill_clock.last_minute<>EXCLUDED.last_minute RETURNING world_id",
        soci::use(world,"w"),soci::use(minute,"minute"),soci::into(claimed);
    if(!sql.got_data()){result.minute_already_processed=true;return result;}
    const std::string due="p.\"bWorldID\"=:w AND p.\"bRead\"=0 AND p.\"bType\"=2 "
        "AND p.\"timeRecv\"+INTERVAL '259200 seconds'<=to_timestamp(:now) AT TIME ZONE 'UTC' "
        "AND NOT EXISTS(SELECT 1 FROM app_global.\"TCURRENTUSER\" u WHERE u.\"bGroupID\"=:w AND u.\"dwCharID\"=p.\"dwCharID\")";
    long long missing=0;
    sql << "SELECT count(*) FROM app_world.\"TPOSTTABLE\" p WHERE "+due+
        " AND NOT EXISTS(SELECT 1 FROM app_world.\"TCHARTABLE\" s WHERE s.\"bWorldID\"=:w AND s.\"dwCharID\"=p.\"dwSendID\")",
        soci::use(world,"w"),soci::use(second,"now"),soci::into(missing);
    if(missing<0||static_cast<unsigned long long>(missing)>UINT32_MAX)
        throw std::runtime_error("Offline bill missing-sender count overflow");
    result.missing_senders=static_cast<std::uint32_t>(missing);
    struct Bill {int post,recipient,sender,user;};std::vector<Bill> bills;
    {
        soci::rowset<soci::row> rows=(sql.prepare <<
            "SELECT p.\"dwPostID\",p.\"dwCharID\",p.\"dwSendID\",c.\"dwUserID\" FROM app_world.\"TPOSTTABLE\" p "
            "JOIN app_world.\"TCHARTABLE\" c ON c.\"bWorldID\"=p.\"bWorldID\" AND c.\"dwCharID\"=p.\"dwCharID\" "
            "WHERE "+due+" AND EXISTS(SELECT 1 FROM app_world.\"TCHARTABLE\" s WHERE s.\"bWorldID\"=:w AND s.\"dwCharID\"=p.\"dwSendID\") "
            "ORDER BY p.\"dwPostID\" LIMIT 128 FOR UPDATE OF p SKIP LOCKED",
            soci::use(world,"w"),soci::use(second,"now"));
        for(const auto& row:rows)bills.push_back({row.get<int>(0),row.get<int>(1),row.get<int>(2),row.get<int>(3)});
    }
    // Login and Map admission take this same account row. Recheck offline
    // status after acquisition, closing the NOT IN cursor's login race.
    // Never wait on an account/mailbox held by a Map that awaits these posts.
    std::set<int> accounts;
    std::set<std::uint32_t> mailboxes;
    for(const auto& b:bills){accounts.insert(b.user);mailboxes.insert(static_cast<std::uint32_t>(b.recipient));mailboxes.insert(static_cast<std::uint32_t>(b.sender));}
    for(const auto account:accounts) {
        int locked=0;
        sql << "SELECT \"dwUserID\" FROM app_global.\"TACCOUNT_PW\" WHERE \"dwUserID\"=:u FOR UPDATE SKIP LOCKED",
            soci::use(account,"u"),soci::into(locked);
        if(!sql.got_data()){result.busy=true;return result;} // rolls minute back
    }
    for(const auto character:mailboxes)if(!TryLock(sql,fourstory::db::NativeMailboxKey(m_world,character))) {
        result.busy=true;return result;
    }
    for(const auto& b:bills) {
        soci::row row;
        sql << "SELECT p.*,encode(sender_bytes,'hex') AS sh,encode(recipient_bytes,'hex') AS rh,"
            "encode(title_bytes,'hex') AS th,encode(message_bytes,'hex') AS mh FROM app_world.\"TPOSTTABLE\" p "
            "WHERE "+due+" AND p.\"dwPostID\"=:id",
            soci::use(world,"w"),soci::use(second,"now"),soci::use(b.post,"id"),soci::into(row);
        if(!sql.got_data())continue; // Login won the account lock before us.
        int recipient_user=0;
        sql << "SELECT \"dwUserID\" FROM app_world.\"TCHARTABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c FOR KEY SHARE",
            soci::use(world,"w"),soci::use(b.recipient,"c"),soci::into(recipient_user);
        if(!sql.got_data()||recipient_user!=b.user)throw std::runtime_error("Offline bill recipient ownership changed");
        int exists=0;
        sql << "SELECT 1 FROM app_world.\"TCHARTABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:sender FOR KEY SHARE",
            soci::use(world,"w"),soci::use(b.sender,"sender"),soci::into(exists);
        if(!sql.got_data())throw std::runtime_error("Offline bill sender disappeared");
        const auto sh=OriginalBytes(row,"sh","szSender"),rh=OriginalBytes(row,"rh","szRecvName"),
            th=OriginalBytes(row,"th","szTitle"),mh=OriginalBytes(row,"mh","szMessage");
        sql << "SET CONSTRAINTS app_world.item_post_recipient DEFERRED";
        int changed=0;
        // TJobsPostReturn retains dwSendID, unlike TPostBillsUpdate's online
        // return. Preserve this source difference instead of quietly swapping it.
        sql << "UPDATE app_world.\"TPOSTTABLE\" SET \"dwCharID\"=\"dwSendID\",\"szRecvName\"=\"szSender\",\"szSender\"=\"szRecvName\","
            "\"bType\"=3,\"bRead\"=0,\"dwGold\"=0,\"dwSilver\"=0,\"dwCooper\"=0,"
            "\"timeRecv\"=to_timestamp(:time) AT TIME ZONE 'UTC',bill_due_at=NULL,"
            "sender_bytes=decode(:rh,'hex'),recipient_bytes=decode(:sh,'hex'),title_bytes=decode(:th,'hex'),message_bytes=decode(:mh,'hex') "
            "WHERE \"bWorldID\"=:w AND \"dwPostID\"=:id AND \"bRead\"=0 AND \"bType\"=2 RETURNING \"dwPostID\"",
            soci::use(stored_second,"time"),soci::use(rh,"rh"),soci::use(sh,"sh"),soci::use(th,"th"),soci::use(mh,"mh"),
            soci::use(world,"w"),soci::use(b.post,"id"),soci::into(changed);
        if(!sql.got_data()||changed!=b.post)throw std::runtime_error("Offline bill ownership changed");
        // The source job only updates obsolete TPOSTITEMTABLE. Native normalized
        // storage2 is sole attachment authority, including graph characters.
        sql << "UPDATE app_world.\"TITEMTABLE\" SET \"dwOwnerID\"=:sender WHERE \"bWorldID\"=:w AND \"dwOwnerID\"=:c "
            "AND \"bOwnerType\"=0 AND \"bStorageType\"=2 AND \"dwStorageID\"=:id",
            soci::use(b.sender,"sender"),soci::use(world,"w"),soci::use(b.recipient,"c"),soci::use(b.post,"id");
        sql << "SET CONSTRAINTS app_world.item_post_recipient IMMEDIATE";
        sql << "UPDATE app_world.mail_notifications n SET superseded_at=clock_timestamp() WHERE n.world_id=:w AND n.post_id=:id "
            "AND n.status='pending' AND n.superseded_at IS NULL AND NOT EXISTS(SELECT 1 FROM app_world.\"TPOSTTABLE\" p "
            "WHERE p.\"bWorldID\"=n.world_id AND p.\"dwPostID\"=n.post_id AND p.\"dwCharID\"=n.char_id AND p.\"bType\"=n.post_type)",
            soci::use(world,"w"),soci::use(b.post,"id");
        sql << "INSERT INTO app_world.mail_notifications(world_id,char_id,post_id,post_type,enqueue_second) VALUES(:w,:c,:p,3,:time)",
            soci::use(world,"w"),soci::use(b.sender,"c"),soci::use(b.post,"p"),soci::use(second,"time");
        sql << "INSERT INTO app_world.offline_bill_returns(world_id,post_id,previous_recipient,new_recipient,source_sender,return_second) "
            "VALUES(:w,:p,:old,:new,:source,:time)",soci::use(world,"w"),soci::use(b.post,"p"),
            soci::use(b.recipient,"old"),soci::use(b.sender,"new"),soci::use(b.sender,"source"),soci::use(second,"time");
        ++result.returned;
    }
    try {tx.commit();}catch(...){m_commit_unknown=true;throw;}
    return result;
}
}
