BEGIN;
SELECT set_config('app.workspace_id','demo-workspace',true);
DO $test$
BEGIN
 IF EXISTS(SELECT 1 FROM application_records) THEN RAISE EXCEPTION 'Data leaked without app/space context'; END IF;
 IF EXISTS(SELECT 1 FROM pg_roles WHERE rolname=current_user AND (rolsuper OR rolbypassrls)) THEN RAISE EXCEPTION 'Overprivileged application role'; END IF;
END $test$;
SELECT set_config('app.application_id',(SELECT id FROM applications ORDER BY created_at,id LIMIT 1),true);
SELECT set_config('app.space_id',(SELECT id FROM application_spaces WHERE app_id=current_setting('app.application_id') ORDER BY created_at,id LIMIT 1),true);
DO $test$
BEGIN
 IF NOT EXISTS(SELECT 1 FROM application_records) THEN RAISE EXCEPTION 'Expected fixture records in the authorized scope'; END IF;
 BEGIN
  DELETE FROM application_events;
  RAISE EXCEPTION 'Audit deletion unexpectedly permitted';
 EXCEPTION WHEN insufficient_privilege THEN NULL;
 END;
 BEGIN
  UPDATE application_versions SET revision=revision;
  RAISE EXCEPTION 'Published version mutation unexpectedly permitted';
 EXCEPTION WHEN insufficient_privilege THEN NULL;
 END;
END $test$;
SELECT set_config('app.space_id','not-an-authorized-space',true);
DO $test$
BEGIN
 IF EXISTS(SELECT 1 FROM application_records) THEN RAISE EXCEPTION 'Cross-space records leaked'; END IF;
 IF EXISTS(SELECT 1 FROM application_assets) THEN RAISE EXCEPTION 'Cross-space assets leaked'; END IF;
END $test$;
SELECT set_config('app.workspace_id','isolation-workspace',true);
DO $test$
BEGIN
 IF EXISTS(SELECT 1 FROM applications) THEN RAISE EXCEPTION 'Cross-workspace application leak'; END IF;
END $test$;
COMMIT;
DO $test$
BEGIN
 IF EXISTS(SELECT 1 FROM applications) OR EXISTS(SELECT 1 FROM application_records) THEN RAISE EXCEPTION 'Transaction context leaked'; END IF;
END $test$;
SELECT 'PASS: app/space RLS, restricted role, immutable versions, append-only activity and context reset' AS result;
