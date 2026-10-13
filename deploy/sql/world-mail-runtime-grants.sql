-- Incremental native World mail permissions after migrations001-065.
-- psql -v world_role=dedicated_world_login; does not certify other World paths.
BEGIN;
GRANT USAGE ON SCHEMA app_world,app_global TO :"world_role";
GRANT SELECT ON app_world.worlds,app_world."TRESERVEDPOST",app_world.reserved_mail_clock,
    app_world."TCHARTABLE",app_world."TPOSTTABLE",app_world.mail_notifications TO :"world_role";
GRANT UPDATE(item_high_water) ON app_world.worlds TO :"world_role";
GRANT UPDATE("bSend",delivered_post_id,delivered_item_id,settled_at) ON app_world."TRESERVEDPOST" TO :"world_role";
GRANT INSERT,UPDATE ON app_world.reserved_mail_clock TO :"world_role";
GRANT INSERT ON app_world."TPOSTTABLE",app_world."TITEMTABLE",app_world.mail_notifications TO :"world_role";
GRANT USAGE ON SEQUENCE app_world."TPOSTTABLE_dwPostID_seq",app_world.mail_notifications_notification_id_seq TO :"world_role";
-- SELECT FOR KEY SHARE on the recipient requires UPDATE permission. Grant
-- only its primary-key column; no INSERT/DELETE or archival access is granted.
GRANT UPDATE("dwCharID") ON app_world."TCHARTABLE" TO :"world_role";
GRANT SELECT("dwUserID") ON app_global."TACCOUNT_PW" TO :"world_role";
GRANT UPDATE("dwUserID") ON app_global."TACCOUNT_PW" TO :"world_role";
GRANT SELECT ON app_global."TCURRENTUSER",app_world."TITEMTABLE",
    app_world.offline_bill_clock,app_world.offline_bill_returns TO :"world_role";
GRANT INSERT,UPDATE ON app_world.offline_bill_clock TO :"world_role";
GRANT INSERT ON app_world.offline_bill_returns TO :"world_role";
GRANT UPDATE("dwCharID","szRecvName","szSender","bType","bRead","dwGold","dwSilver","dwCooper","timeRecv",
    sender_bytes,recipient_bytes,title_bytes,message_bytes,bill_due_at) ON app_world."TPOSTTABLE" TO :"world_role";
GRANT UPDATE("dwOwnerID") ON app_world."TITEMTABLE" TO :"world_role";
GRANT UPDATE(superseded_at) ON app_world.mail_notifications TO :"world_role";
COMMIT;
