-- Run as schema owner: psql -v peer_role=DEDICATED_SERVER_ROLE -f this_file.
-- Apply to each existing service role which uses PeerSecurityGate. Provision
-- peer identities/secrets separately as the owner; this file creates none.
BEGIN;
GRANT USAGE ON SCHEMA runtime_control TO :"peer_role";
GRANT SELECT ON runtime_control.peer_trust TO :"peer_role";
GRANT UPDATE (fail_count,ok_count,last_seen) ON runtime_control.peer_trust TO :"peer_role";
GRANT INSERT (group_id,server_id,type_id,remote_ip,outcome,reason) ON runtime_control.peer_auth_log TO :"peer_role";
GRANT USAGE ON SEQUENCE runtime_control.peer_auth_log_id_seq TO :"peer_role";
COMMIT;
-- Retention DELETE is an operator permission, not a game-service grant.
