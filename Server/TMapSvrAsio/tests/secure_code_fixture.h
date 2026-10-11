#pragma once
void VerifySecureCode(soci::session& admin,SessionPool& pool,tmapsvr::PostgreSQLMapService& source,
    const char* connection,const char* manifest,const char* routing,const char* actor,tmapsvr::MapSessionClaim primary,bool graph) {
    using namespace tmapsvr;const auto uid=std::to_string(primary.user_id),cid=std::to_string(primary.char_id);
    admin<<"UPDATE app_world.\"TCHARTABLE\" SET \"bLevel\"=60,\"dwGold\"=11,\"wSkillPoint\"=200 WHERE \"dwCharID\"="+cid;
    primary.endpoint_ip=0x0100007f;primary.endpoint_port=5815;
    Check(source.ClaimSession(primary,*source.LookupSession(primary.user_id,primary.key)).has_value(),"security primary claim");
    auto live=*source.LoadAuthorized(primary);live.bStartAct=0;live.wMapID=0;live.fPosX=4080;live.fPosY=80;live.fPosZ=3584;source.MarkReady(primary,live);
    auto active=primary;auto* service=&source;
    const auto req=[](std::uint16_t op,std::string code={},std::string next={}){std::vector<std::byte> b;
        if(op!=0x5586&&op!=0x5592)wire::WriteString(b,code);if(op==0x5582)wire::WriteString(b,next);return b;};
    const auto update=[&](std::uint16_t op,std::string code={},std::string next={}) {
        auto result=service->UpdateSecureCode(active,op,req(op,code,next),live);live=*result.snapshot;return result.packet;};
    Check(!update(0x5590,"NoCode00"),"absent security request stays silent");
    const auto empty=live;auto ack=update(0x5580,"Secure000");
    Check(ack->first==0x5581&&ack->second[0]==std::byte{2}&&live.payload->security.unlocked,"create commits account code and session unlock");
    Check(Throws([&]{source.SaveAuthorized(primary,empty);}),"pre-create snapshot cannot save over security state");
    for(int field:{0,1,2}){auto wrong=active;if(field==0)++wrong.connection_id;else if(field==1)++wrong.authority_epoch;else wrong.role=MapSessionRole::Replica;
        Check(Throws([&]{service->UpdateSecureCode(wrong,0x5582,req(0x5582,"Secure000","Changed00"),live);}),"security rejects stale generation epoch and replica");}
    if(Number(admin,"SELECT count(*) FROM character_compat.trainer_release")==1) {
        std::vector<std::byte> purchase;wire::WritePOD<std::uint16_t>(purchase,22047);wire::WritePOD<std::uint16_t>(purchase,101);
        const auto security=live.payload->security;const auto gold=live.dwGold;
        auto learned=source.PurchaseSkill(primary,purchase,live);live=*learned.snapshot;
        Check(learned.packets.back().first==0x52b3&&learned.packets.back().second[0]==std::byte{0}&&live.dwGold<gold,
              "unlocked secured account commits positive native skill purchase and currency charge");
        Check(live.payload->security==security,"native learning preserves account code unlock and revision");
    }
    const auto original=live;const auto count=Number(admin,"SELECT count(*) FROM app_world.secure_code_operations WHERE user_id="+uid);
    admin<<"CREATE TRIGGER synthetic_security_fault BEFORE INSERT ON app_world.map_checkpoints FOR EACH ROW EXECUTE FUNCTION public.reject_checkpoint()";
    Check(Throws([&]{update(0x5582,"Secure000","Changed00");}),"late security checkpoint error rejects operation");
    admin<<"DROP TRIGGER synthetic_security_fault ON app_world.map_checkpoints";
    Check(Number(admin,"SELECT count(*) FROM app_world.secure_code_operations WHERE user_id="+uid)==count,"late failure rolls back secret and operation receipt");
    auto race=[&]{return service->UpdateSecureCode(active,0x5582,req(0x5582,"Secure000","Changed00"),live);};
    auto one=std::async(std::launch::async,[&]()->std::optional<SecureCommit>{try{return race();}catch(...){return {};}});
    auto two=std::async(std::launch::async,[&]()->std::optional<SecureCommit>{try{return race();}catch(...){return {};}});
    auto a=one.get(),b=two.get();Check(bool(a)!=bool(b),"concurrent security writes have one revision winner");live=*(a?a->snapshot:b->snapshot);
    Check(live.payload->security.code=="Changed00","successful retry after known rollback stores new bytes");
    update(0x5584,"Changed00");Check(live.payload->security.disabled,"disable is persistent account state");
    std::unique_ptr<SyntheticReplicaPartition> partition;std::unique_ptr<PostgreSQLMapOwner> owner;std::unique_ptr<PostgreSQLMapService> target;
    if(graph) {
        partition=std::make_unique<SyntheticReplicaPartition>(admin);owner=std::make_unique<PostgreSQLMapOwner>(connection,1,2);
        target=std::make_unique<PostgreSQLMapService>(pool,PostgreSQLMapConfig{1,2,owner->Token(),manifest,routing,actor});
        Check(source.AuthorizeReplicas(primary,0,4080,3584,{{0x0100007f,5815,2}}),"secure transfer authorizes replica");
        auto secondary=primary;secondary.connection_id+=1000;
        Check(target->ClaimSession(secondary,*target->LookupSession(primary.user_id,primary.key)).has_value(),"secure transfer claims replica");
        secondary.role=MapSessionRole::Replica;Check(target->LoadReplica(secondary,0,4080,3584),"secure transfer loads replica");target->MarkReady(secondary,live);
        live.fPosX=4100;SkillCooldownTracker timers;timers.Restore(primary.char_id,live.payload->skills,0);
        const auto body=transfer::Encode(transfer::Capture(live,primary.key,timers,0));
        Check(source.PrepareTransfer(primary,live,body),"secure full graph prepares with original fields");const auto received=target->AcceptTransfer(secondary,body);
        Check(received.has_value(),"secure target promotes");active=secondary;active.role=MapSessionRole::Primary;active.authority_epoch=received->authority_epoch;
        live=received->snapshot;service=target.get();service->MarkReady(active,live);
        Check(live.payload->security.disabled&&live.payload->security.unlocked&&live.payload->security.code=="Changed00","disabled and same-login unlock survive omitted source transfer flag");
        Check(Throws([&]{source.UpdateSecureCode(primary,0x5586,{},original);}),"old primary cannot enable security after transfer");
    }
    update(0x5586);Check(!live.payload->security.disabled,"enable preserves same-session unlock");
    for(int n=1;n<=10;++n){ack=update(0x5590,"incorrect");Check(ack->first==0x5591&&ack->second==std::vector<std::byte>({std::byte(n==10?10:7),std::byte(n)}),"CHECK has original attempt count and threshold code");}
    auto locked=live;ack=update(0x5582,"Changed00","Refused00");
    Check(ack->first==0x5591&&ack->second[0]==std::byte{10}&&live.payload->security==locked.payload->security,"locked correct code cannot mutate before GETLOCKTIME reset");
    ack=update(0x5592);Check(ack->first==0x5593&&ack->second[0]==std::byte{23},"lock time sends original hour/minute bytes");
    // Advance the persisted fixture clock to test expiry without a 24h wait.
    const auto deadline=live.payload->security.deadline_ms-86400001;
    admin<<"UPDATE app_global.secure_codes SET deadline_ms=:d WHERE user_id="+uid,soci::use(deadline);
    auto p=std::make_shared<CharacterPayload>(*live.payload);p->security.deadline_ms=deadline;live.payload=p;
    ack=update(0x5592);Check(ack->first==0x5589&&ack->second==std::vector<std::byte>({std::byte{0},std::byte{0}})&&!live.payload->security.tries,"GETLOCKTIME durably clears expired attempts");
    Check(Throws([&]{service->SaveAuthorized(active,locked);}),"old locked state cannot resurrect expired attempts");
    service->SaveAuthorized(active,live);
    {auto l=pool.Acquire();Check(Throws([&]{*l<<"DELETE FROM app_global.secure_codes WHERE false";}),"Map role cannot delete account security");
     Check(Throws([&]{*l<<"UPDATE app_world.secure_code_operations SET response_opcode=0 WHERE false";}),"Map role cannot rewrite secure receipts");}
}
