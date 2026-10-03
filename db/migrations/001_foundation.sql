BEGIN;
SELECT pg_advisory_xact_lock(740013001);
CREATE TABLE IF NOT EXISTS schema_migrations (version integer PRIMARY KEY, applied_at timestamptz NOT NULL DEFAULT now());
CREATE TABLE IF NOT EXISTS workspaces (id text PRIMARY KEY, name text NOT NULL);
CREATE TABLE IF NOT EXISTS runs (
  workspace_id text NOT NULL REFERENCES workspaces(id), id text NOT NULL,
  idempotency_key text NOT NULL CHECK(length(idempotency_key) BETWEEN 16 AND 128),
  input_hash text NOT NULL, request jsonb NOT NULL, document jsonb NOT NULL DEFAULT '{}',
  state text NOT NULL DEFAULT 'queued' CHECK(state IN ('queued','preparing','awaiting_approval','executing','completed','failed','cancelled','needs_reconciliation')),
  version integer NOT NULL DEFAULT 1 CHECK(version > 0), proposal_hash text NOT NULL DEFAULT '',
  expires_at timestamptz, created_at timestamptz NOT NULL DEFAULT now(),
  PRIMARY KEY(workspace_id,id), UNIQUE(workspace_id,idempotency_key)
);
CREATE TABLE IF NOT EXISTS jobs (
  workspace_id text NOT NULL, run_id text NOT NULL, kind text NOT NULL CHECK(kind IN ('prepare','simulate')),
  created_at timestamptz NOT NULL DEFAULT now(), PRIMARY KEY(workspace_id,run_id,kind),
  FOREIGN KEY(workspace_id,run_id) REFERENCES runs(workspace_id,id)
);
CREATE TABLE IF NOT EXISTS run_events (
  workspace_id text NOT NULL, run_id text NOT NULL, sequence bigint GENERATED ALWAYS AS IDENTITY,
  kind text NOT NULL, detail jsonb NOT NULL DEFAULT '{}', created_at timestamptz NOT NULL DEFAULT now(),
  PRIMARY KEY(workspace_id,run_id,sequence), FOREIGN KEY(workspace_id,run_id) REFERENCES runs(workspace_id,id)
);
CREATE TABLE IF NOT EXISTS apps (
  workspace_id text NOT NULL REFERENCES workspaces(id), id text NOT NULL, title text NOT NULL,
  request jsonb NOT NULL, created_at timestamptz NOT NULL DEFAULT now(), PRIMARY KEY(workspace_id,id)
);
CREATE INDEX IF NOT EXISTS runs_recent ON runs(workspace_id,created_at DESC);
CREATE INDEX IF NOT EXISTS jobs_ready ON jobs(workspace_id,created_at);
ALTER TABLE workspaces ENABLE ROW LEVEL SECURITY;
ALTER TABLE workspaces FORCE ROW LEVEL SECURITY;
DROP POLICY IF EXISTS workspace_scope ON workspaces;
CREATE POLICY workspace_scope ON workspaces USING (id = current_setting('app.workspace_id',true)) WITH CHECK (id = current_setting('app.workspace_id',true));
DO $block$
DECLARE t text;
BEGIN
  FOREACH t IN ARRAY ARRAY['runs','jobs','run_events','apps'] LOOP
    EXECUTE format('ALTER TABLE %I ENABLE ROW LEVEL SECURITY',t);
    EXECUTE format('ALTER TABLE %I FORCE ROW LEVEL SECURITY',t);
    EXECUTE format('DROP POLICY IF EXISTS tenant_scope ON %I',t);
    EXECUTE format('CREATE POLICY tenant_scope ON %I USING (workspace_id = current_setting(''app.workspace_id'',true)) WITH CHECK (workspace_id = current_setting(''app.workspace_id'',true))',t);
  END LOOP;
END $block$;
INSERT INTO workspaces(id,name) VALUES ('demo-workspace','Development workspace'),('isolation-workspace','Isolation fixture') ON CONFLICT DO NOTHING;
GRANT USAGE ON SCHEMA public TO runtime_app;
GRANT SELECT ON schema_migrations,workspaces TO runtime_app;
GRANT SELECT,INSERT,UPDATE ON runs,apps TO runtime_app;
GRANT SELECT,INSERT,DELETE ON jobs TO runtime_app;
GRANT SELECT,INSERT ON run_events TO runtime_app;
GRANT USAGE ON ALL SEQUENCES IN SCHEMA public TO runtime_app;
INSERT INTO schema_migrations(version) VALUES (1) ON CONFLICT DO NOTHING;
COMMIT;
