BEGIN;
SELECT set_config('app.workspace_id','demo-workspace',true);
DO $test$
DECLARE n integer;
BEGIN
  SELECT count(*) INTO n FROM workspaces;
  IF n <> 1 THEN RAISE EXCEPTION 'Tenant isolation failed'; END IF;
  IF EXISTS(SELECT 1 FROM workspaces WHERE id='isolation-workspace') THEN RAISE EXCEPTION 'Foreign workspace visible'; END IF;
  BEGIN
    INSERT INTO runs(workspace_id,id,idempotency_key,input_hash,request) VALUES('isolation-workspace','denied-rls-test','denied-rls-test-key','test','{}');
    RAISE EXCEPTION 'Cross-tenant insertion unexpectedly succeeded';
  EXCEPTION WHEN insufficient_privilege THEN NULL;
  END;
  IF EXISTS(SELECT 1 FROM pg_roles WHERE rolname=current_user AND (rolsuper OR rolbypassrls)) THEN RAISE EXCEPTION 'Overprivileged app role'; END IF;
  BEGIN
    DELETE FROM run_events;
    RAISE EXCEPTION 'Audit deletion unexpectedly succeeded';
  EXCEPTION WHEN insufficient_privilege THEN NULL;
  END;
END $test$;
COMMIT;
DO $test$
BEGIN
  IF EXISTS(SELECT 1 FROM workspaces) THEN RAISE EXCEPTION 'Tenant context leaked after transaction'; END IF;
END $test$;
BEGIN;
SELECT set_config('app.workspace_id','isolation-workspace',true);
DO $test$
BEGIN
  IF EXISTS(SELECT 1 FROM workspaces WHERE id='demo-workspace') THEN RAISE EXCEPTION 'Reverse tenant leak'; END IF;
END $test$;
ROLLBACK;
SELECT 'PASS: RLS, transaction-local context, app role, audit immutability' AS result;
