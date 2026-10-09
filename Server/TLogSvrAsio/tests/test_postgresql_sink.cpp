// Owned disposable schema only; the runner creates the fixture and fault proxy.
#include "services/log_sink.h"
#include "services/audit_query_repository.h"
#include "db/schema_validator.h"
#include <boost/asio/thread_pool.hpp>
#include <soci/soci.h>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>
#include <chrono>
using namespace tlogsvr;
using namespace fourstory::db;
using namespace std::chrono_literals;
namespace {
unsigned passed=0,failed=0;
void Check(bool ok,const char* label) { std::printf("%s %s\n",ok?"PASS":"FAIL",label);ok?++passed:++failed; }
LogRecord Record(unsigned action) {
    LogRecord r; r.timestamp_iso="2026-10-09 12:34:56";r.server_id=0xffffffff;r.client_ip="127.0.0.1";
    r.action=action;r.map_id=65535;r.pos_x=-2147483647;r.pos_y=2147483647;r.pos_z=-13;
    r.format=0xffffffff;
    for(unsigned i=0;i<11;++i)r.search_int[i]=-9223372036854775807LL+i;
    for(auto& s:r.search_str)s="apostrophe' and \\ slash";
    for(unsigned i=0;i<512;++i)r.payload.push_back(static_cast<std::byte>(i%256));
    return r;
}
long long Count(soci::session& sql) { long long n=0;sql<<"SELECT count(*) FROM \"TLOG_AUDIT\"",soci::into(n);return n; }
void Clear(soci::session& sql) { sql<<"TRUNCATE \"TLOG_AUDIT\""; }
SociLogSink::Options Options() { SociLogSink::Options o;o.max_retry_queue=8;o.drain_batch_size=10000;return o; }
void QueueTwo(SociLogSink& sink,SessionPool& pool) {
    auto held=pool.Acquire(); sink.Write(Record(10));sink.Write(Record(77));
    Check(sink.QueueDepth()==2,"pool exhaustion buffers two records without sending writes");
}
void Normal(const char* conn,soci::session& admin) {
    SessionPool pool(Backend::PostgreSQL,conn,1,10ms);
    tlogsvr::db::ValidateAuditSchema(pool,"TLOG_AUDIT");Check(true,"native schema validator verifies migration 029 contract and types");
    bool unsafe=false;try {SociLogSink s(pool,"bad\"table");}catch(const std::invalid_argument&){unsafe=true;}
    Check(unsafe,"sink rejects unsafe identifier independently of startup");
    Clear(admin);SociLogSink sink(pool,"TLOG_AUDIT",Options());sink.Write(Record(0xffffffff));
    Check(sink.Inserts()==1&&Count(admin)==1,"single transaction commits once");
    std::string blob,text;long long server=0,action=0,format=0,number=0;int map=0,x=0;
    admin<<"SELECT encode(lt_log,'hex'),encode(lt_key1,'hex'),lt_serverid,lt_action,lt_fmt,lt_mapid,lt_x,lt_dwkey11 FROM \"TLOG_AUDIT\"",
        soci::into(blob),soci::into(text),soci::into(server),soci::into(action),soci::into(format),soci::into(map),soci::into(x),soci::into(number);
    std::string expected;constexpr char hex[]="0123456789abcdef";for(unsigned i=0;i<512;++i){expected+=hex[(i%256)>>4];expected+=hex[i&15];}
    Check(blob==expected,"all 256 byte values and embedded NUL round-trip in single insert");
    Check(AuditUnhex(text)==Record(1).search_str[0],"quotes and backslashes remain data");
    Check(server==4294967295LL&&action==4294967295LL&&format==4294967295LL&&map==65535,"DWORD and WORD maxima are not sign-truncated");
    Check(x==-2147483647&&number==-9223372036854775797LL,"signed positions and 64-bit search keys round-trip");
    auto empty=Record(3);empty.payload.clear();sink.Write(empty);
    int nulls=0;admin<<"SELECT count(*) FROM \"TLOG_AUDIT\" WHERE lt_log IS NULL",soci::into(nulls);
    Check(nulls==1,"empty payload preserves existing NULL contract");
    Clear(admin);QueueTwo(sink,pool);
    admin<<"ALTER TABLE \"TLOG_AUDIT\" ADD CONSTRAINT injected_rejection CHECK(lt_action<>77)";
    sink.DrainPending();
    Check(Count(admin)==0&&sink.QueueDepth()==2&&sink.UnknownOutcomes()==0,"rejected batch is atomic and confirmed rollback retains entire queue");
    admin<<"ALTER TABLE \"TLOG_AUDIT\" DROP CONSTRAINT injected_rejection";
    sink.DrainPending();
    Check(Count(admin)==2&&sink.QueueDepth()==0&&sink.DrainedAfterRetry()==2,"safe retry commits batch once");
    int binary_rows=0;admin<<"SELECT count(*) FROM \"TLOG_AUDIT\" WHERE octet_length(lt_log)=512 AND get_byte(lt_log,0)=0 AND get_byte(lt_log,255)=255",soci::into(binary_rows);
    Check(binary_rows==2,"batch binary binding matches single binding");
    // Multi-threaded arrivals and drains must not make PushFront exceed capacity.
    Clear(admin);admin<<"ALTER TABLE \"TLOG_AUDIT\" ADD CONSTRAINT injected_rejection CHECK(lt_action<>77)";
    SociLogSink blocked(pool,"TLOG_AUDIT",Options());
    std::vector<std::thread> threads;
    for(int t=0;t<4;++t)threads.emplace_back([&]{for(int i=0;i<8;++i)blocked.Write(Record(77));});
    threads.emplace_back([&]{for(int i=0;i<10;++i)blocked.DrainPending();});
    for(auto& t:threads)t.join();
    Check(blocked.QueueDepth()==8&&blocked.DroppedQueueFull()==24,"concurrent rejected drains keep the hard queue bound");
    Check(Count(admin)==0&&blocked.UnknownOutcomes()==0,"concurrent failure creates no durable rows or false unknowns");
    admin<<"ALTER TABLE \"TLOG_AUDIT\" DROP CONSTRAINT injected_rejection";blocked.DrainPending();
    Check(Count(admin)==8&&blocked.DrainedAfterRetry()==8,"accepted concurrent backlog drains exactly once");
    Clear(admin);
    {
        SociLogSink worker(pool,"TLOG_AUDIT",Options());boost::asio::thread_pool jobs(1);worker.SetWorkerPool(&jobs);
        for(unsigned i=0;i<32;++i)worker.Write(Record(i));
        jobs.join();
        Check(worker.Inserts()==32&&Count(admin)==32,"graceful worker join completes all accepted jobs");
    }
}
void NativeReads(const char* conn,soci::session& admin) {
    Clear(admin);SessionPool pool(Backend::PostgreSQL,conn,1,10ms);
    SociLogSink sink(pool,"TLOG_AUDIT",Options());AuditQueryRepository repo(pool);
    auto first=Record(17);first.search_int[0]=-17;
    for(auto& key:first.search_str){key.clear();for(int i=128;i<178;++i)key+=static_cast<char>(i);}
    first.client_ip=std::string("ip\xff",3);
    sink.Write(first);auto second=Record(18);second.search_int[0]=42;sink.Write(second);
    first.action=19;sink.Write(first);
    Check(sink.Inserts()==3&&repo.Count()==3,"native repository counts committed records under restricted role");
    auto rows=repo.LatestN(2);
    Check(rows.size()==2&&rows[0].action==19&&rows[1].action==18&&rows[0].log_id>rows[1].log_id,"native LIMIT and identity ordering");
    Check(rows[0].search_str_0==first.search_str[0]&&rows[0].client_ip==first.client_ip,"native reads preserve invalid UTF-8 bytes without guessing encoding");
    Check(rows[0].server_id==0xffffffff&&rows[0].format==0xffffffff&&rows[0].map_id==65535,"native reads preserve full unsigned wire ranges");
    Check(rows[0].log_date==first.timestamp_iso,"native timestamp formats to original seconds without timezone conversion");
    rows=repo.WhereUserId(-17,10);
    Check(rows.size()==2&&rows[0].action==19&&rows[1].action==17,"bound signed user query preserves ordering");
    Check(repo.WhereUserId(42,1).size()==1&&repo.WhereUserId(99,5).empty(),"native user filter and limit");
    Check(repo.LatestN(0).empty()&&repo.WhereUserId(-17,0).empty(),"zero limit returns no records");
    int exact=0;
    admin<<"SELECT count(*) FROM \"TLOG_AUDIT\" WHERE lt_action IN (17,19) AND lt_key1=lt_key2 AND lt_key2=lt_key3 "
           "AND lt_key3=lt_key4 AND lt_key4=lt_key5 AND lt_key5=lt_key6 AND lt_key6=lt_key7 AND octet_length(lt_key7)=50",soci::into(exact);
    Check(exact==2,"all seven full-width CHAR fields persist exact bytes");
    admin<<"CREATE TABLE public.\"TLOG_AUDIT\" (lt_id integer)";
    sink.Write(Record(20));Check(repo.Count()==4,"public shadow cannot redirect fully-qualified native write or read");
    admin<<"DROP TABLE public.\"TLOG_AUDIT\"";
    for(const auto& command:{"UPDATE app_audit.\"TLOG_AUDIT\" SET lt_action=0", "DELETE FROM app_audit.\"TLOG_AUDIT\"",
                            "TRUNCATE app_audit.\"TLOG_AUDIT\"", "UPDATE app_audit.runtime_contract SET version=1",
                            "CREATE TABLE app_audit.forbidden(id integer)","SELECT * FROM app_global.\"TACCOUNT_PW\""}) {
        bool refused=false;{auto lease=pool.Acquire();try{*lease<<command;}catch(const soci::soci_error&){refused=true;}}
        Check(refused,"runtime audit role cannot mutate audit history/schema or read player accounts");
    }
    auto invalid=Record(21);invalid.search_str[3]=std::string(51,'x');
    {SociLogSink rejected(pool,"TLOG_AUDIT",Options());rejected.Write(invalid);
     Check(rejected.QueueDepth()==1&&rejected.UnknownOutcomes()==0&&repo.Count()==4,"over-width raw key is atomically rejected, never truncated");}
    std::string user;{auto lease=pool.Acquire();*lease<<"SELECT current_user",soci::into(user);}
    if(!tlogsvr::db::IsSafeAuditIdentifier(user))throw std::runtime_error("Unexpected test role");
    admin<<"CREATE TABLE app_audit.\"TLOG_ALT\" (LIKE app_audit.\"TLOG_AUDIT\" INCLUDING ALL)";
    admin<<"GRANT SELECT,INSERT ON app_audit.\"TLOG_ALT\" TO \""+user+"\"";
    admin<<"GRANT USAGE ON SEQUENCE app_audit.\"TLOG_ALT_lt_id_seq\" TO \""+user+"\"";
    {SociLogSink alternate(pool,"TLOG_ALT",Options());AuditQueryRepository alternate_repo(pool,"TLOG_ALT");
     tlogsvr::db::ValidateAuditSchema(pool,"TLOG_ALT");alternate.Write(Record(22));
     Check(alternate_repo.Count()==1&&alternate_repo.LatestN(1).at(0).action==22&&repo.Count()==4,"configured native table used consistently by sink, validator and reader");}
    admin<<"REVOKE INSERT ON app_audit.\"TLOG_ALT\" FROM \""+user+"\"";
    bool no_insert=false;try{tlogsvr::db::ValidateAuditSchema(pool,"TLOG_ALT");}catch(...){no_insert=true;}
    Check(no_insert,"native boot validation refuses missing INSERT privileges");
    admin<<"GRANT INSERT ON app_audit.\"TLOG_ALT\" TO \""+user+"\"";
    admin<<"REVOKE USAGE ON SEQUENCE app_audit.\"TLOG_ALT_lt_id_seq\" FROM \""+user+"\"";
    bool no_sequence=false;try{tlogsvr::db::ValidateAuditSchema(pool,"TLOG_ALT");}catch(...){no_sequence=true;}
    Check(no_sequence,"native boot validation refuses missing identity allocation grant");
    admin<<"GRANT USAGE ON SEQUENCE app_audit.\"TLOG_ALT_lt_id_seq\" TO \""+user+"\"";
    admin<<"ALTER TABLE app_audit.\"TLOG_ALT\" ALTER COLUMN lt_key7 TYPE text USING encode(lt_key7,'hex')";
    bool refused=false;try{tlogsvr::db::ValidateAuditSchema(pool,"TLOG_ALT");}catch(...){refused=true;}
    Check(refused,"validator refuses incompatible text column instead of silently accepting it");
    admin<<"DROP TABLE app_audit.\"TLOG_ALT\"";Clear(admin);
}
void LostCommit(const char* conn,soci::session& admin,bool batch) {
    Clear(admin);SessionPool pool(Backend::PostgreSQL,conn,1,10ms);SociLogSink sink(pool,"TLOG_AUDIT",Options());
    if(batch){QueueTwo(sink,pool);sink.DrainPending();}else sink.Write(Record(10));
    const unsigned count=batch?2:1;
    Check(Count(admin)==count,"proxy proves database committed before reply was lost");
    Check(sink.UnknownOutcomes()==count&&sink.Inserts()==0&&sink.DrainedAfterRetry()==0,"lost commit counts uncertain records separately");
    Check(sink.QueueDepth()==0,"unknown commit is never queued");
    for(int i=0;i<3;++i)sink.DrainPending();
    Check(Count(admin)==count,"repeated drain cannot duplicate uncertain commit");
    bool closed=false;{auto lease=pool.Acquire();try{*lease<<"SELECT 1";}catch(...){closed=true;}}
    Check(closed,"uncertain connection is discarded before pool release");
}
void LostInsert(const char* conn,soci::session& admin) {
    Clear(admin);SessionPool pool(Backend::PostgreSQL,conn,1,10ms);SociLogSink sink(pool,"TLOG_AUDIT",Options());sink.Write(Record(9));
    Check(Count(admin)==0,"disconnect before COMMIT leaves no durable insert");
    Check(sink.UnknownOutcomes()==1&&sink.QueueDepth()==0,"unconfirmed rollback is surfaced without blind retry");
}
}
int main() {
    const auto* conn=std::getenv("TLOGSVR_TEST_POSTGRESQL_CONN");
    const auto* control_conn=std::getenv("TLOGSVR_TEST_ADMIN_CONN");
    const auto* lost=std::getenv("TLOGSVR_TEST_LOST_COMMIT_CONN");
    const auto* insert=std::getenv("TLOGSVR_TEST_LOST_INSERT_CONN");
    if(!conn||!control_conn||!lost||!insert){std::puts("SKIP: owned PostgreSQL/proxy fixture required");return 77;}
    try {
        SessionPool control(Backend::PostgreSQL,control_conn,1);auto admin=control.Acquire();
        Normal(conn,*admin);NativeReads(conn,*admin);LostCommit(lost,*admin,false);LostCommit(lost,*admin,true);LostInsert(insert,*admin);Clear(*admin);
    }catch(...){Check(false,"unexpected fixture failure (diagnostics intentionally omit SQL/credentials)");}
    std::printf("Results: %u passed, %u failed\n",passed,failed);return failed?1:0;
}
