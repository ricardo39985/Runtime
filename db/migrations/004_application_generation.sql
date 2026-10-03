BEGIN;
SELECT pg_advisory_xact_lock(740013001);
CREATE TABLE application_generations (
 workspace_id text NOT NULL REFERENCES workspaces(id), id text NOT NULL, principal_id text NOT NULL,
 request_key text NOT NULL, input_hash text NOT NULL, state text NOT NULL CHECK(state IN ('dispatched','complete','invalid','unknown')),
 request jsonb NOT NULL, result jsonb NOT NULL DEFAULT '{}', charged_microusd bigint NOT NULL CHECK(charged_microusd>=0),
 created_at timestamptz NOT NULL DEFAULT now(), PRIMARY KEY(workspace_id,id), UNIQUE(workspace_id,principal_id,request_key)
);
CREATE INDEX application_generation_spend ON application_generations(workspace_id,created_at);
ALTER TABLE application_generations ENABLE ROW LEVEL SECURITY;
ALTER TABLE application_generations FORCE ROW LEVEL SECURITY;
CREATE POLICY workspace_scope ON application_generations USING(workspace_id=current_setting('app.workspace_id',true)) WITH CHECK(workspace_id=current_setting('app.workspace_id',true));
GRANT SELECT,INSERT,UPDATE ON application_generations TO runtime_app;
INSERT INTO schema_migrations(version) VALUES(4);
COMMIT;
