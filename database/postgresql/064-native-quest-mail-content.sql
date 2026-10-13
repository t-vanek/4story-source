-- Additive original quest SENDPOST publication; no historical row is rewritten.
ALTER TABLE runtime_control.quest_condition_catalog
    ADD COLUMN quest_mail boolean NOT NULL DEFAULT false;
ALTER TABLE runtime_control.quest_condition_activations
    ADD COLUMN quest_mail boolean NOT NULL DEFAULT false;
CREATE VIEW game_compat.quest_mail_release AS
    SELECT r.* FROM game_compat.quest_reward_release r
    JOIN runtime_control.quest_condition_catalog c ON c.run_id=r.run_id
    WHERE c.singleton AND c.quest_mail;
CREATE VIEW game_compat.quest_mail_text AS
    SELECT s."dwQuestID",s."_source_row_number",
        s."szTitle" IS NOT NULL AND s."szMessage" IS NOT NULL AND s."szNPCName" IS NOT NULL AS valid,
        substring(n.source_bytes FROM 1 FOR 255) AS sender_bytes,
        substring(convert_to(lpad(to_hex(octet_length(t.source_bytes)),8,'0'),'UTF8')
            ||t.source_bytes FROM 1 FOR 255) AS title_bytes,
        substring(convert_to(lpad(to_hex(octet_length(m.source_bytes)),8,'0'),'UTF8')
            ||m.source_bytes FROM 1 FOR 1024) AS message_bytes
    FROM legacy_game."TQTITLECHART" s
    JOIN game_compat.quest_mail_release r ON r.run_id=s."_import_run_id"
    JOIN reconstruction.source_text_bytes t ON t.run_id=s."_import_run_id"
        AND t.source_database='TGAME_RAGEZONE' AND t.source_schema='dbo'
        AND t.source_table='TQTITLECHART' AND t.source_row_number=s."_source_row_number" AND t.column_name='szTitle'
    JOIN reconstruction.source_text_bytes m ON m.run_id=s."_import_run_id"
        AND m.source_database='TGAME_RAGEZONE' AND m.source_schema='dbo'
        AND m.source_table='TQTITLECHART' AND m.source_row_number=s."_source_row_number" AND m.column_name='szMessage'
    JOIN reconstruction.source_text_bytes n ON n.run_id=s."_import_run_id"
        AND n.source_database='TGAME_RAGEZONE' AND n.source_schema='dbo'
        AND n.source_table='TQTITLECHART' AND n.source_row_number=s."_source_row_number" AND n.column_name='szNPCName';
REVOKE ALL ON game_compat.quest_mail_release,game_compat.quest_mail_text FROM PUBLIC;
COMMENT ON VIEW game_compat.quest_mail_text IS
    'Pinned backup TQTITLECHART uses varchar255/1024/1024, Latin1_General_CI_AS_KS. Preserve original source bytes: TQuestSendPost truncates sender to255, prefixes pre-assignment byte lengths, then truncates title/message to255/1024 again. NULL title/message/sender is source return2, not synthesized text.';
