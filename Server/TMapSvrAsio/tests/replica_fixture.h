#pragma once
// Explicit synthetic partition in a disposable test DB. Original imported rows
// are untouched; the exact compatibility view definition is restored on scope exit.
struct SyntheticReplicaPartition {
    soci::session& sql;
    std::string definition;
    explicit SyntheticReplicaPartition(soci::session& db):sql(db) {
        sql<<"SELECT pg_get_viewdef('route_compat.\"TSVRCHART\"'::regclass,true)",soci::into(definition);
        auto inner=definition;while(!inner.empty()&&(inner.back()==';'||inner.back()=='\n'||inner.back()==' '))inner.pop_back();
        sql<<"CREATE OR REPLACE VIEW route_compat.\"TSVRCHART\" AS SELECT release_id,\"bGroup\","
             "CASE WHEN \"wMapID\"=0 AND \"wUnitID\"=772 AND \"bChannel\"=1 THEN 2::smallint ELSE \"bServerID\" END AS \"bServerID\","
             "\"wMapID\",\"wUnitID\",\"bChannel\" FROM ("+inner+") original";
    }
    ~SyntheticReplicaPartition(){try{sql<<"CREATE OR REPLACE VIEW route_compat.\"TSVRCHART\" AS "+definition;}catch(...){std::terminate();}}
};
void VerifyReplicas(soci::session& admin,SessionPool& pool,tmapsvr::PostgreSQLMapService& main,
    const char* connection,const char* manifest,const char* routing,const char* actor,const tmapsvr::MapSessionClaim& primary) {
    SyntheticReplicaPartition partition(admin);
    admin<<"INSERT INTO app_global.\"TSERVER\" VALUES(1,2,4,1,5815) ON CONFLICT DO NOTHING";
    auto owner=std::make_unique<tmapsvr::PostgreSQLMapOwner>(connection,1,2);
    auto config=[&]{return tmapsvr::PostgreSQLMapConfig{1,2,owner->Token(),manifest,routing,actor};};
    tmapsvr::PostgreSQLMapService replica(pool,config());
    Check(main.ClaimSession(primary,*main.LookupSession(primary.user_id,primary.key)).has_value(),"replica fixture primary claimed");
    auto snap=main.LoadAuthorized(primary);snap->wMapID=0;snap->fPosX=4080;snap->fPosY=80;snap->fPosZ=3584;
    const std::vector<tmapsvr::ServerRoute> endpoints{{0x0100007f,5815,2}};
    Check(main.NeighborServers(primary,0,4080,3584)==std::optional<std::vector<std::uint8_t>>{{2}},"explicit synthetic partition resolves neighboring Map");
    auto secondary=primary;secondary.connection_id=100;secondary.endpoint_ip=0x0100007f;secondary.endpoint_port=5815;
    auto claim=[&](const tmapsvr::MapSessionClaim& c){return replica.ClaimSession(c,*replica.LookupSession(primary.user_id,primary.key));};
    Check(!claim(secondary),"ungranted replica cannot reuse Login handoff");
    auto wrong=endpoints;wrong[0].server_id=3;
    Check(!main.AuthorizeReplicas(primary,0,4080,3584,wrong),"non-neighbor cannot receive a replica grant");
    wrong=endpoints;wrong[0].port++;
    Check(!main.AuthorizeReplicas(primary,0,4080,3584,wrong),"unpublished endpoint cannot receive a grant");
    auto stale=primary;stale.connection_id++;
    Check(!main.AuthorizeReplicas(stale,0,4080,3584,endpoints),"stale primary cannot authorize replicas");
    Check(main.AuthorizeReplicas(primary,0,4080,3584,endpoints),"primary authorizes exact one-use replica endpoint");
    const auto expiry=Number(admin,"SELECT (extract(epoch FROM expires_at)*1000000)::bigint FROM app_world.map_replicas");
    Check(main.AuthorizeReplicas(primary,0,4080,3584,endpoints)&&Number(admin,"SELECT (extract(epoch FROM expires_at)*1000000)::bigint FROM app_world.map_replicas")==expiry,"repeated grant does not extend expiry");
    stale=secondary;stale.endpoint_port++;
    Check(!claim(stale),"wrong replica endpoint cannot consume grant");
    stale=secondary;stale.channel++;
    Check(!claim(stale),"wrong replica channel cannot consume grant");
    admin<<"UPDATE app_world.map_replicas SET expires_at=clock_timestamp()-interval '1 second'";
    Check(!claim(secondary),"expired replica grant rejected");
    Check(main.AuthorizeReplicas(primary,0,4080,3584,endpoints),"new route decision can replace expired unconsumed grant");
    admin<<"UPDATE app_world.map_replicas SET primary_connection=primary_connection+1";
    Check(!claim(secondary),"grant from stale primary generation rejected");
    admin<<"UPDATE app_world.map_replicas SET primary_connection=primary_connection-1";
    std::barrier gate(3);std::optional<tmapsvr::MapSessionInfo> results[2];auto other=secondary;other.connection_id++;
    std::thread one([&]{gate.arrive_and_wait();results[0]=claim(secondary);});
    std::thread two([&]{gate.arrive_and_wait();results[1]=claim(other);});
    gate.arrive_and_wait();one.join();two.join();
    Check(bool(results[0])!=bool(results[1]),"concurrent replica claims have exactly one winner");
    if(results[1])secondary=other;
    secondary.role=tmapsvr::MapSessionRole::Replica;
    Check((results[0]?results[0]:results[1])->role==tmapsvr::MapSessionRole::Replica,"consumption returns explicit replica role");
    Check(!claim(secondary),"consumed replica grant cannot be replayed");
    Check(!replica.LoadAuthorized(secondary),"replica cannot reload stale durable primary graph");
    Check(!replica.OwnsCell(secondary,0,4080,3584),"replica cannot claim primary cell ownership");
    Check(Throws([&]{replica.MarkReady(secondary,*snap);}),"replica cannot become ready before composite hydration");
    Check(!replica.LoadReplica(secondary,0,4079,3584),"wrong replica composite location rejected");
    Check(replica.LoadReplica(secondary,0,4080,3584),"replica validates source composite against grant");
    replica.MarkReady(secondary,*snap);
    Check(Number(admin,"SELECT count(*) FROM app_world.map_replicas WHERE phase='ready'")==1,"replica reaches ready without primary core write");
    Check(Number(admin,"SELECT count(*) FROM app_world.map_sessions WHERE user_id=710 AND phase='loaded'")==1,"replica readiness does not promote primary");
    Check(Throws([&]{replica.SaveAuthorized(secondary,*snap);})&&Throws([&]{replica.CheckpointAuthorized(secondary,*snap,1);}),"replica cannot save or checkpoint primary state");
    main.MarkReady(primary,*snap);
    stale=secondary;stale.connection_id+=5;replica.ReleaseSession(stale);
    Check(Number(admin,"SELECT count(*) FROM app_world.map_replicas WHERE phase='ready'")==1,"stale replica teardown cannot retire current generation");
    replica.ReleaseSession(secondary);
    Check(Number(admin,"SELECT count(*) FROM app_world.map_replicas WHERE phase='retired'")==1&&Number(admin,"SELECT count(*) FROM app_global.\"TCURRENTUSER\" WHERE \"dwUserID\"=710")==1,"replica retirement preserves primary account reservation");
    Check(!claim(secondary),"retired replica grant cannot be replayed");
    Check(main.AuthorizeReplicas(primary,0,4080,3584,endpoints),"new source route decision renews retired replica");
    secondary.connection_id+=10;Check(claim(secondary).has_value(),"renewed replica uses a fresh connection generation");
    Check(replica.LoadReplica(secondary,0,4080,3584),"renewed replica composite loads before target restart");
    replica.MarkReady(secondary,*snap);
    owner.reset();owner=std::make_unique<tmapsvr::PostgreSQLMapOwner>(connection,1,2);
    tmapsvr::PostgreSQLMapService replacement(pool,config());
    Check(Number(admin,"SELECT count(*) FROM app_world.map_replicas")==0&&Number(admin,"SELECT count(*) FROM app_world.map_sessions WHERE user_id=710 AND phase='ready'")==1,"target restart cleans only its replica token and preserves primary");
    Check(Throws([&]{replica.ReleaseSession(secondary);}),"replaced replica process cannot write");
    Check(!replacement.ClaimSession(secondary,*replacement.LookupSession(primary.user_id,primary.key)),"target restart cannot reuse an old grant");
    Check(main.AuthorizeReplicas(primary,0,4080,3584,endpoints),"primary can grant the replacement target token");
    Check(replacement.ClaimSession(secondary,*replacement.LookupSession(primary.user_id,primary.key)).has_value(),"replacement consumes newly authorized target grant");
    main.SaveAuthorized(primary,*snap);
    Check(Number(admin,"SELECT count(*) FROM app_world.map_replicas")==0&&Number(admin,"SELECT count(*) FROM app_global.\"TCURRENTUSER\" WHERE \"dwUserID\"=710")==0,"primary final save atomically cascades replica grants and account release");
    replacement.ReleaseSession(secondary);
    Check(Throws([&]{replacement.MarkReady(secondary,*snap);}),"deleted primary cannot be revived by delayed replica readiness");
}
