-- Written only; use existing Map/World grants plus076 during Phase B.
BEGIN;
GRANT USAGE ON SCHEMA route_compat TO :"world_role";
GRANT SELECT ON route_compat.map_topology_release,route_compat.source_map_regions,
    route_compat.source_map_channels TO :"map_role",:"world_role";
GRANT SELECT("bGroupID","bServerID","bType") ON app_global."TSERVER" TO :"world_role";
COMMIT;
