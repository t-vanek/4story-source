-- Security belongs to the account (backup TSECURECODE.dwUserID), not the
-- lifetime of a particular character. Original native low-level character
-- deletion must not erase the code or its audit, or be blocked by that audit.
-- Keep the historical character identifier in the receipt without a live FK.
-- Migration042 has already been exercised; preserve it and upgrade additively.
ALTER TABLE app_world.secure_code_operations
 DROP CONSTRAINT secure_code_operations_world_id_char_id_fkey;
COMMENT ON COLUMN app_world.secure_code_operations.char_id IS
 'Historical character that performed the account operation; intentionally retained after character deletion. The account FK and account-wide revision chain remain authoritative.';
