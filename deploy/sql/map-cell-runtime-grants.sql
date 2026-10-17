-- Written only. Requires existing Map grants and migration081 in Phase B.
BEGIN;
GRANT SELECT ON route_compat.cell_catalog_release,route_compat.source_cell_channels,
    route_compat.source_cell_owners TO :"map_role";
COMMIT;
