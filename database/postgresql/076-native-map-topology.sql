-- Phase A: written only. Existing complete routing publication is the source;
-- these views neither rewrite the archive nor create synthetic channel rows.
CREATE VIEW route_compat.map_topology_release AS
    SELECT w.group_id AS world_id,w.source_group,r.run_id,r.manifest_sha256
    FROM app_world.routing_worlds w CROSS JOIN route_compat.catalog_release r
    WHERE r.status='verified' AND
        (SELECT count(*) FROM reconstruction.checkpoints c WHERE c.run_id=r.run_id
            AND c.source_database='TGAME_RAGEZONE' AND c.source_schema='dbo'
            AND c.source_table IN ('TMAPCHART','TUNITCHART','TCHANNELCHART','TSPAWNPOSCHART')
            AND c.source_sha256=c.target_sha256
            AND c.source_bytes_sha256=c.target_bytes_sha256)=4;
CREATE VIEW route_compat.source_map_regions AS
    SELECT s.* FROM route_compat."TSVRCHART" s;
-- CTBLChannelChart joins by group/map/unit/physical channel and groups the
-- selected columns. Original WHERE is server-only, without a group parameter.
CREATE VIEW route_compat.source_map_channels AS
    SELECT DISTINCT c.release_id,c."bGroupID",c."wMapID",c."bLogChannel",c."bPhyChannel",s."bServerID"
    FROM route_compat."TCHANNELCHART" c JOIN route_compat."TSVRCHART" s
        ON c."wMapID"=s."wMapID" AND c."bGroupID"=s."bGroup"
        AND c."wUnitID"=s."wUnitID" AND c."bPhyChannel"=s."bChannel";
ALTER TABLE app_world.map_runtime_owner
    ADD COLUMN routing_manifest text,
    ADD COLUMN routing_source_group smallint,
    ADD COLUMN routing_topology jsonb,
    ADD CONSTRAINT map_owner_topology_shape CHECK (
        (routing_manifest IS NULL AND routing_source_group IS NULL AND routing_topology IS NULL) OR
        (routing_manifest IS NOT NULL AND routing_manifest ~ '^[0-9a-f]{64}$'
            AND routing_source_group IS NOT NULL AND routing_source_group BETWEEN 0 AND 255
            AND routing_topology IS NOT NULL AND jsonb_typeof(routing_topology)='array'
            AND jsonb_array_length(routing_topology)=2));
COMMENT ON COLUMN app_world.map_runtime_owner.routing_topology IS
 'Process-start source topology [physical BYTE channels, ordered [DWORD MAKELONG(map,logical channel),physical BYTE] rows]. Not an MW_CONNECT_ACK body. Prior NULL rows do not prove topology; no historical backfill.';
CREATE TABLE runtime_control.routing_owner_retirements (
    activation_id bigint NOT NULL REFERENCES runtime_control.routing_catalog_activations(id),
    world_id smallint NOT NULL,
    server_id smallint NOT NULL,
    retired_token text NOT NULL CHECK(retired_token ~ '^[0-9a-f]{64}$'),
    replacement_token text NOT NULL CHECK(replacement_token ~ '^[0-9a-f]{64}$'),
    PRIMARY KEY(activation_id,world_id,server_id)
);
COMMENT ON TABLE runtime_control.routing_owner_retirements IS
 'Offline routing activation retires old Map process tokens, including workers that outlive their advisory-lock backend. No player graph or historical row is rewritten.';
REVOKE ALL ON route_compat.map_topology_release,route_compat.source_map_regions,
    route_compat.source_map_channels,runtime_control.routing_owner_retirements FROM PUBLIC;
