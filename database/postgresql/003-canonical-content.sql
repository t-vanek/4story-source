-- Migration 003: canonical content projection (PROPOSED, evidence-linked).
-- Full source columns remain in immutable versioned legacy snapshots.
CREATE SCHEMA content;
CREATE TABLE content.releases (
    id bigint PRIMARY KEY REFERENCES reconstruction.import_runs(id),
    game_version text, -- UNKNOWN; backup date is not a game version
    server_protocol integer CHECK (server_protocol BETWEEN 0 AND 65535),
    source_database text NOT NULL
);
CREATE TABLE content.item_templates (
    release_id bigint NOT NULL REFERENCES content.releases(id),
    item_id integer NOT NULL CHECK (item_id BETWEEN 0 AND 65535),
    display_name text NOT NULL,
    item_type smallint NOT NULL CHECK (item_type BETWEEN 0 AND 255),
    item_kind smallint NOT NULL CHECK (item_kind BETWEEN 0 AND 255),
    max_stack smallint NOT NULL CHECK (max_stack BETWEEN 0 AND 255),
    price double precision NOT NULL, -- source is FLOAT, not an invented money unit
    PRIMARY KEY (release_id, item_id)
);
COMMENT ON TABLE content.item_templates IS
  'PROPOSED canonical projection of TGAME_RAGEZONE.dbo.TITEMCHART; source wItemID is a protocol WORD';
