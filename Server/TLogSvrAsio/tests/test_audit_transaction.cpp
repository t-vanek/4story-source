#include "services/audit_transaction.h"
#include <cstdio>
#include <stdexcept>
#include <string>
using namespace tlogsvr;
struct Session {
    bool begin_fail=false, rollback_fail=false, commit_fail=false, close_fail=false;
    std::string trace;
    void begin() { trace+='B'; if(begin_fail) throw std::runtime_error("begin"); }
    void rollback() { trace+='R'; if(rollback_fail) throw std::runtime_error("rollback"); }
    void commit() { trace+='C'; if(commit_fail) throw std::runtime_error("commit"); }
    void close() { trace+='X'; if(close_fail) throw std::runtime_error("close"); }
};
int main() {
    unsigned passed=0,failed=0;
    auto check=[&](bool ok,const char* label){std::printf("%s %s\n",ok?"PASS":"FAIL",label);ok?++passed:++failed;};
    for(bool bulk:{false,true}) {
        Session sql;
        auto insert=[&] { sql.trace+=bulk?"II":"I"; };
        check(RunAuditTransaction(sql,insert)==AuditWriteOutcome::Committed,"confirmed commit");
        check(sql.trace==(bulk?"BIIC":"BIC"),"begin precedes writes and one commit");
        sql={};sql.begin_fail=true;
        check(RunAuditTransaction(sql,insert)==AuditWriteOutcome::Retryable&&sql.trace=="BX","begin failure cannot insert");
        sql={};
        auto fail=[&]{insert();throw std::runtime_error("insert");};
        check(RunAuditTransaction(sql,fail)==AuditWriteOutcome::Retryable,"confirmed rollback permits retry");
        check(sql.trace==(bulk?"BIIR":"BIR"),"insert failure rolls back without commit");
        sql={};sql.rollback_fail=true;
        check(RunAuditTransaction(sql,fail)==AuditWriteOutcome::Unknown,"failed rollback is not silently retried");
        check(sql.trace==(bulk?"BIIRX":"BIRX"),"failed rollback discards connection");
        sql={};sql.commit_fail=true;
        check(RunAuditTransaction(sql,insert)==AuditWriteOutcome::Unknown,"lost commit confirmation is unknown");
        check(sql.trace==(bulk?"BIICX":"BICX"),"rollback after uncertain commit is never used as proof");
        sql={};sql.commit_fail=true;sql.close_fail=true;
        check(RunAuditTransaction(sql,insert)==AuditWriteOutcome::Unknown,"close failure cannot change uncertain result");
    }
    std::printf("Results: %u passed, %u failed\n",passed,failed);
    return failed?1:0;
}
