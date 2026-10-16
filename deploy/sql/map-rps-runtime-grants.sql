-- Written only; apply with the existing Map grants after075 in Phase B.
BEGIN;
GRANT SELECT ON game_compat.rps_release,game_compat."TRPSGAMECHART" TO :"map_role";
GRANT SELECT(post_id) ON app_world."TPOSTERRORTABLE" TO :"map_role";
COMMIT;
