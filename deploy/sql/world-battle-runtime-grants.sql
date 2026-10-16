-- Phase A: written only. Apply after077 with the existing World grants.
BEGIN;
GRANT USAGE ON SCHEMA world_compat TO :"world_role";
GRANT SELECT ON world_compat.battle_release,world_compat."TBATTLETIMECHART",
    world_compat."TCUSTOMTIMECHART" TO :"world_role";
COMMIT;
