-- Native offline TJobsPostReturn replacement, not an archival rewrite.
CREATE TABLE app_world.offline_bill_clock (
    world_id smallint PRIMARY KEY REFERENCES app_world.worlds(group_id),
    last_minute bigint NOT NULL,
    updated_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
CREATE TABLE app_world.offline_bill_returns (
    world_id smallint NOT NULL REFERENCES app_world.worlds(group_id),
    post_id integer NOT NULL,
    previous_recipient integer NOT NULL,
    new_recipient integer NOT NULL,
    source_sender integer NOT NULL,
    return_second bigint NOT NULL CHECK(return_second>=0),
    completed_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    PRIMARY KEY(world_id,post_id)
);
CREATE INDEX native_offline_unread_bills ON app_world."TPOSTTABLE"("bWorldID","timeRecv","dwPostID")
    WHERE "bRead"=0 AND "bType"=2;
REVOKE ALL ON app_world.offline_bill_clock,app_world.offline_bill_returns FROM PUBLIC;
COMMENT ON TABLE app_world.offline_bill_returns IS
    'Native atomic offline bill return audit. Original job uses offline recipient, stored receive time+3 days, retains dwSendID and returns type3/funds0. Repair obsolete TPOSTITEMTABLE writes by moving authoritative STORAGE_POST item owners in the same transaction. No receipt or old delivery history is invented for historical data.';
COMMENT ON TABLE app_world.offline_bill_clock IS
    'Native World minute scheduler, maximum128 returns per batch; original SQL Agent scheduling metadata is unavailable. Account row locks serialize Login admission, sorted nonblocking mailbox locks serialize Maps. Missing senders retain their post/items for reconciliation; unknown commit stops the worker.';
