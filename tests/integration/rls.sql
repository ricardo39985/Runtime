BEGIN;
SELECT set_config('app.workspace_id','demo-workspace',true);
DO $test$
BEGIN
 IF NOT EXISTS(SELECT 1 FROM applications) THEN RAISE EXCEPTION 'Run API acceptance before isolation checks'; END IF;
 IF EXISTS(SELECT 1 FROM workspaces WHERE id='isolation-workspace') THEN RAISE EXCEPTION 'Foreign workspace visible'; END IF;
 IF EXISTS(SELECT 1 FROM pg_roles WHERE rolname=current_user AND (rolsuper OR rolbypassrls)) THEN RAISE EXCEPTION 'Overprivileged role'; END IF;
 BEGIN
  INSERT INTO applications(workspace_id,id,app_key,name,version,spec) VALUES('isolation-workspace','forbidden','forbidden','Forbidden',1,'{}');
  RAISE EXCEPTION 'Cross-tenant insertion succeeded';
 EXCEPTION WHEN insufficient_privilege THEN NULL;
 END;
 BEGIN
  DELETE FROM application_events;
  RAISE EXCEPTION 'Audit deletion succeeded';
 EXCEPTION WHEN insufficient_privilege THEN NULL;
 END;
 BEGIN
  UPDATE application_objects SET content='changed';
  RAISE EXCEPTION 'Immutable object mutation succeeded';
 EXCEPTION WHEN insufficient_privilege THEN NULL;
 END;
END $test$;
COMMIT;
DO $test$
BEGIN
 IF EXISTS(SELECT 1 FROM applications) THEN RAISE EXCEPTION 'Tenant context survived commit'; END IF;
END $test$;
BEGIN;
SELECT set_config('app.workspace_id','isolation-workspace',true);
DO $test$
BEGIN
 IF EXISTS(SELECT 1 FROM applications) OR EXISTS(SELECT 1 FROM application_records) OR EXISTS(SELECT 1 FROM application_files) OR EXISTS(SELECT 1 FROM application_objects) OR EXISTS(SELECT 1 FROM application_events) THEN RAISE EXCEPTION 'Application data leaked across tenants'; END IF;
END $test$;
ROLLBACK;
SELECT 'PASS application RLS, transaction context, immutable assets and audit' AS result;
