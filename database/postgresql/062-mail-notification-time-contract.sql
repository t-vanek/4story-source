-- Source OnMW_POSTRECV_REQ carries no timestamp: the receiving Map uses its
-- current m_timeCurrent. Keep the enqueue second as audit metadata only.
-- Preserve written061 and its values, without drop/recreate or historical edits.
ALTER TABLE app_world.mail_notifications RENAME COLUMN received_at TO enqueue_second;
COMMENT ON COLUMN app_world.mail_notifications.enqueue_second IS
    'Enqueue-side live second for native receipt provenance only. Never sent as POSTRECV time: source OnMW_POSTRECV_REQ/OnDM_POSTRECV_ACK use the receiving Map current second when handling delivery. TPOST.timeRecv remains separately minute-rounded.';
