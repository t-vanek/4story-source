#include "server_messages.h"
#include "postgresql_world_owner.h"
#include <soci/soci.h>
#include <algorithm>
#include <bit>

namespace tworldsvr {
ServerMessages LoadPostgreSQLServerMessages(fourstory::db::SessionPool& pool,std::uint8_t world,
    const std::string& token,const std::string& encoding) {
    try {
        auto lease=pool.Acquire();auto& sql=*lease;auto tx=BeginWorldTransaction(sql,world,token,true);
        int compatible=0;
        // Reuse the full pinned offline --server-messages publication from059.
        // Never activate a latest run, a reference subset or a made-up name.
        sql<<"SELECT count(*) FROM game_compat.server_message_release r "
             "JOIN game_compat.catalog_release m ON m.manifest_sha256=r.map_manifest AND m.status='verified' "
             "JOIN character_compat.catalog_release c ON c.manifest_sha256=r.character_manifest AND c.status='verified' "
             "JOIN actor_compat.catalog_release a ON a.manifest_sha256=r.actor_manifest AND a.status='verified' "
             "JOIN reconstruction.checkpoints p ON p.run_id=r.run_id AND p.source_database='TGAME_RAGEZONE' "
             "AND p.source_schema='dbo' AND p.source_table='TSVRMSGCHART' AND p.row_count=51 "
             "AND length(p.source_sha256)=64 AND p.source_sha256=p.target_sha256 "
             "AND length(p.source_bytes_sha256)=64 AND p.source_bytes_sha256=p.target_bytes_sha256 "
             "WHERE r.status='verified'",soci::into(compatible);
        if(compatible!=1)throw std::runtime_error("Full verified server message publication required");
        ServerMessages messages;
        // Copy out the rowset before issuing encoding probes on this session.
        {soci::rowset<soci::row> rows=(sql.prepare<<
            "SELECT \"dwID\",encode(message_bytes,'hex') FROM game_compat.server_messages");
         for(const auto& row:rows) {
            if(row.get_indicator(0)!=soci::i_ok||row.get_indicator(1)!=soci::i_ok)
                throw std::runtime_error("NULL source server message");
            const auto hex=row.get<std::string>(1);std::string bytes;
            if(hex.size()%2)throw std::runtime_error("Invalid server message bytes");
            bytes.reserve(hex.size()/2);
            for(std::size_t n=0;n<hex.size();n+=2)bytes+=static_cast<char>(std::stoul(hex.substr(n,2),nullptr,16));
            if(bytes.size()>256||bytes.find('\0')!=std::string::npos||
               !messages.emplace(std::bit_cast<std::uint32_t>(static_cast<std::int32_t>(row.get<int>(0))),bytes).second)
                throw std::runtime_error("Invalid or ambiguous original server message");
         }}
        if(messages.size()!=51||!messages.contains(kOperatorNameMessage))
            throw std::runtime_error("Incomplete server message byte projection or required World text");
        for(const auto& [id,bytes]:messages) {
            (void)id;
            if(std::all_of(bytes.begin(),bytes.end(),[](unsigned char b){return b<128;}))continue;
            if(encoding.empty())throw std::runtime_error("Explicit ANSI World text profile required");
            constexpr char digits[]="0123456789abcdef";std::string hex;
            for(unsigned char byte:bytes){hex+=digits[byte>>4];hex+=digits[byte&15];}
            std::string roundtrip,probe;
            sql<<"SELECT encode(convert_to(convert_from(decode(:h,'hex'),:e),:e),'hex'),"
                 "encode(convert_to('Mail',:e),'hex')",soci::use(hex,"h"),soci::use(encoding,"e"),
                 soci::into(roundtrip),soci::into(probe);
            if(roundtrip!=hex||probe!="4d61696c")throw std::runtime_error("Lossless ANSI World message profile required");
        }
        tx->commit();return messages;
    } catch(...) {throw std::runtime_error("Native World message bootstrap failed; no fabricated text fallback");}
}
}
