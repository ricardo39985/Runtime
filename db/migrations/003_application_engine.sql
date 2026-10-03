BEGIN;
SELECT pg_advisory_xact_lock(740013001);
CREATE TABLE IF NOT EXISTS applications (
 workspace_id text NOT NULL REFERENCES workspaces(id), id text NOT NULL,
 app_key text NOT NULL, name text NOT NULL, version integer NOT NULL CHECK(version>0), spec jsonb NOT NULL,
 created_at timestamptz NOT NULL DEFAULT now(), updated_at timestamptz NOT NULL DEFAULT now(),
 PRIMARY KEY(workspace_id,id), UNIQUE(workspace_id,app_key)
);
CREATE TABLE IF NOT EXISTS application_versions (
 workspace_id text NOT NULL, app_id text NOT NULL, version integer NOT NULL, spec jsonb NOT NULL,
 created_at timestamptz NOT NULL DEFAULT now(), PRIMARY KEY(workspace_id,app_id,version),
 FOREIGN KEY(workspace_id,app_id) REFERENCES applications(workspace_id,id)
);
CREATE TABLE IF NOT EXISTS application_records (
 workspace_id text NOT NULL, app_id text NOT NULL, entity text NOT NULL, id text NOT NULL,
 version integer NOT NULL DEFAULT 1 CHECK(version>0), data jsonb NOT NULL,
 idempotency_key text NOT NULL, request_hash text NOT NULL, archived boolean NOT NULL DEFAULT false,
 created_at timestamptz NOT NULL DEFAULT now(), updated_at timestamptz NOT NULL DEFAULT now(),
 PRIMARY KEY(workspace_id,app_id,entity,id), UNIQUE(workspace_id,app_id,idempotency_key),
 FOREIGN KEY(workspace_id,app_id) REFERENCES applications(workspace_id,id)
);
CREATE TABLE IF NOT EXISTS application_objects (
 workspace_id text NOT NULL, app_id text NOT NULL, sha256 text NOT NULL CHECK(length(sha256)=64),
 codec text NOT NULL CHECK(codec IN ('identity','zstd')),
 original_size bigint NOT NULL CHECK(original_size BETWEEN 0 AND 8388608),
 stored_size bigint NOT NULL CHECK(stored_size BETWEEN 0 AND 8388608), content bytea NOT NULL,
 PRIMARY KEY(workspace_id,app_id,sha256), CHECK(octet_length(content)=stored_size),
 FOREIGN KEY(workspace_id,app_id) REFERENCES applications(workspace_id,id)
);
CREATE TABLE IF NOT EXISTS application_files (
 workspace_id text NOT NULL, app_id text NOT NULL, id text NOT NULL, sha256 text NOT NULL,
 filename text NOT NULL, declared_mime text NOT NULL, idempotency_key text NOT NULL,
 created_at timestamptz NOT NULL DEFAULT now(), PRIMARY KEY(workspace_id,app_id,id),
 UNIQUE(workspace_id,app_id,idempotency_key),
 FOREIGN KEY(workspace_id,app_id,sha256) REFERENCES application_objects(workspace_id,app_id,sha256)
);
CREATE TABLE IF NOT EXISTS application_record_links (
 workspace_id text NOT NULL, app_id text NOT NULL, entity text NOT NULL, record_id text NOT NULL, field text NOT NULL,
 target_entity text NOT NULL, target_id text NOT NULL,
 PRIMARY KEY(workspace_id,app_id,entity,record_id,field),
 FOREIGN KEY(workspace_id,app_id,entity,record_id) REFERENCES application_records(workspace_id,app_id,entity,id),
 FOREIGN KEY(workspace_id,app_id,target_entity,target_id) REFERENCES application_records(workspace_id,app_id,entity,id)
);
CREATE TABLE IF NOT EXISTS application_file_links (
 workspace_id text NOT NULL, app_id text NOT NULL, entity text NOT NULL, record_id text NOT NULL, field text NOT NULL, file_id text NOT NULL,
 PRIMARY KEY(workspace_id,app_id,entity,record_id,field),
 FOREIGN KEY(workspace_id,app_id,entity,record_id) REFERENCES application_records(workspace_id,app_id,entity,id),
 FOREIGN KEY(workspace_id,app_id,file_id) REFERENCES application_files(workspace_id,app_id,id)
);
CREATE TABLE IF NOT EXISTS application_events (
 workspace_id text NOT NULL, app_id text NOT NULL, sequence bigint GENERATED ALWAYS AS IDENTITY,
 kind text NOT NULL, detail jsonb NOT NULL, created_at timestamptz NOT NULL DEFAULT now(),
 PRIMARY KEY(workspace_id,app_id,sequence), FOREIGN KEY(workspace_id,app_id) REFERENCES applications(workspace_id,id)
);
CREATE INDEX IF NOT EXISTS application_records_page ON application_records(workspace_id,app_id,entity,id) WHERE NOT archived;
CREATE INDEX IF NOT EXISTS application_links_target ON application_record_links(workspace_id,app_id,target_entity,target_id);
DO $block$
DECLARE t text;
BEGIN
 FOREACH t IN ARRAY ARRAY['applications','application_versions','application_records','application_objects','application_files','application_record_links','application_file_links','application_events'] LOOP
  EXECUTE format('ALTER TABLE %I ENABLE ROW LEVEL SECURITY',t);
  EXECUTE format('ALTER TABLE %I FORCE ROW LEVEL SECURITY',t);
  EXECUTE format('DROP POLICY IF EXISTS tenant_scope ON %I',t);
  EXECUTE format('CREATE POLICY tenant_scope ON %I USING (workspace_id=current_setting(''app.workspace_id'',true)) WITH CHECK (workspace_id=current_setting(''app.workspace_id'',true))',t);
 END LOOP;
END $block$;
GRANT SELECT,INSERT,UPDATE ON applications,application_records TO runtime_app;
GRANT SELECT,INSERT ON application_versions,application_objects,application_files,application_events TO runtime_app;
GRANT SELECT,INSERT,DELETE ON application_record_links,application_file_links TO runtime_app;
GRANT USAGE ON ALL SEQUENCES IN SCHEMA public TO runtime_app;
INSERT INTO schema_migrations(version) VALUES (3) ON CONFLICT DO NOTHING;
COMMIT;
