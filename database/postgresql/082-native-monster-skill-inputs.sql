-- Phase A: written only. Derived readers; no historical or operational rows
-- are changed. Existing079 publication is reused, with exact extra source proofs.
CREATE VIEW monster_compat.skill_release AS
    SELECT c.* FROM monster_compat.catalog_release c
    JOIN character_compat.catalog_release ch ON ch.manifest_sha256=c.character_manifest AND ch.status='verified'
    JOIN game_compat.catalog_release g ON g.manifest_sha256=c.content_manifest AND g.status='verified'
    WHERE (SELECT count(*) FROM (VALUES
        (ch.run_id,'TSKILLCHART'),(ch.run_id,'TFORMULACHART'),
        (g.run_id,'TCLASSCHART'),(g.run_id,'TRACECHART')) d(run_id,source_table)
        JOIN reconstruction.checkpoints f ON f.run_id=c.run_id AND f.source_database='TGAME_RAGEZONE'
            AND f.source_schema='dbo' AND f.source_table=d.source_table
        JOIN reconstruction.checkpoints a ON a.run_id=d.run_id AND a.source_database=f.source_database
            AND a.source_schema=f.source_schema AND a.source_table=f.source_table
            AND a.row_count=f.row_count AND a.source_sha256=f.source_sha256
            AND a.target_sha256=f.source_sha256 AND f.target_sha256=f.source_sha256
            AND a.source_bytes_sha256 IS NOT DISTINCT FROM f.source_bytes_sha256
            AND a.target_bytes_sha256 IS NOT DISTINCT FROM f.source_bytes_sha256
            AND f.target_bytes_sha256 IS NOT DISTINCT FROM f.source_bytes_sha256)=4;
CREATE VIEW monster_compat."TSKILLCHART" AS
    SELECT c.world_id,c.server_id,s.* FROM monster_compat.skill_release c
    JOIN legacy_game."TSKILLCHART" s ON s."_import_run_id"=c.run_id;
CREATE VIEW monster_compat."TFORMULACHART" AS
    SELECT c.world_id,c.server_id,s.* FROM monster_compat.skill_release c
    JOIN legacy_game."TFORMULACHART" s ON s."_import_run_id"=c.run_id;
CREATE VIEW monster_compat.skill_class_stats AS
    SELECT c.world_id,c.server_id,s."bClassID",s."wSTR",s."wDEX",s."wCON",s."wINT",s."wWIS",s."wMEN"
    FROM monster_compat.skill_release c JOIN legacy_game."TCLASSCHART" s ON s."_import_run_id"=c.run_id;
CREATE VIEW monster_compat.skill_race_stats AS
    SELECT c.world_id,c.server_id,s."bRaceID",s."wSTR",s."wDEX",s."wCON",s."wINT",s."wWIS",s."wMEN"
    FROM monster_compat.skill_release c JOIN legacy_game."TRACECHART" s ON s."_import_run_id"=c.run_id;
COMMENT ON VIEW monster_compat.skill_release IS
 'Exact full079 source plus active character skill/formula and content class/race value+byte equality. Missing formula or class objects remain distinguishable in C++; no skill, clock, stat or live host rows are seeded.';
REVOKE ALL ON monster_compat.skill_release,monster_compat."TSKILLCHART",monster_compat."TFORMULACHART",
    monster_compat.skill_class_stats,monster_compat.skill_race_stats FROM PUBLIC;
