-- Written only: apply after070 with existing native Map runtime grants.
BEGIN;
GRANT USAGE ON SCHEMA death_compat TO :"map_role";
GRANT SELECT ON death_compat.catalog_release,death_compat."TBATTLEZONECHART",
    death_compat."TGODBALLCHART" TO :"map_role";
COMMIT;
