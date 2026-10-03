BEGIN;
SELECT pg_advisory_xact_lock(740013001);
CREATE TABLE applications (
 workspace_id text NOT NULL REFERENCES workspaces(id), id text NOT NULL, name text NOT NULL,
 creator_id text NOT NULL, request_key text NOT NULL, input_hash text NOT NULL, head_revision integer NOT NULL DEFAULT 1, created_at timestamptz NOT NULL DEFAULT now(),
 PRIMARY KEY(workspace_id,id), UNIQUE(workspace_id,creator_id,request_key)
);
CREATE TABLE application_versions (
 workspace_id text NOT NULL, app_id text NOT NULL, revision integer NOT NULL CHECK(revision>0), spec jsonb NOT NULL,
 created_at timestamptz NOT NULL DEFAULT now(), PRIMARY KEY(workspace_id,app_id,revision),
 FOREIGN KEY(workspace_id,app_id) REFERENCES applications(workspace_id,id)
);
CREATE TABLE application_spaces (
 workspace_id text NOT NULL, app_id text NOT NULL, id text NOT NULL, name text NOT NULL,
 created_at timestamptz NOT NULL DEFAULT now(), PRIMARY KEY(workspace_id,app_id,id),
 FOREIGN KEY(workspace_id,app_id) REFERENCES applications(workspace_id,id)
);
CREATE TABLE application_memberships (
 workspace_id text NOT NULL, app_id text NOT NULL, space_id text NOT NULL, principal_id text NOT NULL, role text NOT NULL,
 PRIMARY KEY(workspace_id,app_id,space_id,principal_id),
 FOREIGN KEY(workspace_id,app_id,space_id) REFERENCES application_spaces(workspace_id,app_id,id)
);
CREATE TABLE application_records (
 workspace_id text NOT NULL, app_id text NOT NULL, space_id text NOT NULL, entity_id text NOT NULL, id text NOT NULL,
 data jsonb NOT NULL, row_version integer NOT NULL DEFAULT 1 CHECK(row_version>0), request_key text NOT NULL,
 input_hash text NOT NULL, created_at timestamptz NOT NULL DEFAULT now(), updated_at timestamptz NOT NULL DEFAULT now(),
 PRIMARY KEY(workspace_id,app_id,space_id,entity_id,id), UNIQUE(workspace_id,app_id,space_id,entity_id,request_key),
 FOREIGN KEY(workspace_id,app_id,space_id) REFERENCES application_spaces(workspace_id,app_id,id)
);
CREATE TABLE application_unique_values (
 workspace_id text NOT NULL, app_id text NOT NULL, space_id text NOT NULL, entity_id text NOT NULL,
 field_id text NOT NULL, value text NOT NULL CHECK(octet_length(value)<=256), record_id text NOT NULL,
 PRIMARY KEY(workspace_id,app_id,space_id,entity_id,field_id,value),
 FOREIGN KEY(workspace_id,app_id,space_id,entity_id,record_id) REFERENCES application_records(workspace_id,app_id,space_id,entity_id,id)
);
CREATE TABLE application_edges (
 workspace_id text NOT NULL, app_id text NOT NULL, space_id text NOT NULL, entity_id text NOT NULL,
 record_id text NOT NULL, field_id text NOT NULL, target_entity text NOT NULL, target_id text NOT NULL,
 PRIMARY KEY(workspace_id,app_id,space_id,entity_id,record_id,field_id),
 FOREIGN KEY(workspace_id,app_id,space_id,entity_id,record_id) REFERENCES application_records(workspace_id,app_id,space_id,entity_id,id),
 FOREIGN KEY(workspace_id,app_id,space_id,target_entity,target_id) REFERENCES application_records(workspace_id,app_id,space_id,entity_id,id)
);
CREATE TABLE application_assets (
 workspace_id text NOT NULL, app_id text NOT NULL, space_id text NOT NULL, id text NOT NULL,
 filename text NOT NULL, media_type text NOT NULL, original_size bigint NOT NULL CHECK(original_size>=0),
 stored_size bigint NOT NULL CHECK(stored_size>=0), sha256 text NOT NULL, codec text NOT NULL,
 state text NOT NULL CHECK(state IN ('pending','ready')), request_key text NOT NULL, input_hash text NOT NULL,
 created_at timestamptz NOT NULL DEFAULT now(), PRIMARY KEY(workspace_id,app_id,space_id,id),
 UNIQUE(workspace_id,app_id,space_id,request_key),
 FOREIGN KEY(workspace_id,app_id,space_id) REFERENCES application_spaces(workspace_id,app_id,id)
);
CREATE TABLE application_record_assets (
 workspace_id text NOT NULL, app_id text NOT NULL, space_id text NOT NULL, entity_id text NOT NULL,
 record_id text NOT NULL, field_id text NOT NULL, asset_id text NOT NULL,
 PRIMARY KEY(workspace_id,app_id,space_id,entity_id,record_id,field_id),
 FOREIGN KEY(workspace_id,app_id,space_id,entity_id,record_id) REFERENCES application_records(workspace_id,app_id,space_id,entity_id,id),
 FOREIGN KEY(workspace_id,app_id,space_id,asset_id) REFERENCES application_assets(workspace_id,app_id,space_id,id)
);
CREATE TABLE application_invocations (
 workspace_id text NOT NULL, app_id text NOT NULL, space_id text NOT NULL, request_key text NOT NULL,
 input_hash text NOT NULL, response jsonb NOT NULL, created_at timestamptz NOT NULL DEFAULT now(),
 PRIMARY KEY(workspace_id,app_id,space_id,request_key),
 FOREIGN KEY(workspace_id,app_id,space_id) REFERENCES application_spaces(workspace_id,app_id,id)
);
CREATE TABLE application_events (
 workspace_id text NOT NULL, app_id text NOT NULL, space_id text NOT NULL, sequence bigint GENERATED ALWAYS AS IDENTITY,
 principal_id text NOT NULL, kind text NOT NULL, detail jsonb NOT NULL, created_at timestamptz NOT NULL DEFAULT now(),
 PRIMARY KEY(workspace_id,app_id,space_id,sequence),
 FOREIGN KEY(workspace_id,app_id,space_id) REFERENCES application_spaces(workspace_id,app_id,id)
);
CREATE INDEX application_records_data ON application_records USING gin(data jsonb_path_ops);
CREATE INDEX application_records_scan ON application_records(workspace_id,app_id,space_id,entity_id,id);
CREATE INDEX application_edges_target ON application_edges(workspace_id,app_id,space_id,target_entity,target_id);
DO $block$
DECLARE t text;
BEGIN
 FOREACH t IN ARRAY ARRAY['applications','application_versions','application_spaces','application_memberships','application_records','application_unique_values','application_edges','application_assets','application_record_assets','application_invocations','application_events'] LOOP
  EXECUTE format('ALTER TABLE %I ENABLE ROW LEVEL SECURITY',t);
  EXECUTE format('ALTER TABLE %I FORCE ROW LEVEL SECURITY',t);
  EXECUTE format('CREATE POLICY workspace_scope ON %I USING (workspace_id=current_setting(''app.workspace_id'',true)) WITH CHECK (workspace_id=current_setting(''app.workspace_id'',true))',t);
 END LOOP;
 FOREACH t IN ARRAY ARRAY['application_records','application_unique_values','application_edges','application_assets','application_record_assets','application_invocations','application_events'] LOOP
  EXECUTE format('CREATE POLICY space_scope ON %I AS RESTRICTIVE USING (app_id=current_setting(''app.application_id'',true) AND space_id=current_setting(''app.space_id'',true)) WITH CHECK (app_id=current_setting(''app.application_id'',true) AND space_id=current_setting(''app.space_id'',true))',t);
 END LOOP;
END $block$;
GRANT SELECT,INSERT,UPDATE ON applications,application_spaces,application_memberships,application_assets TO runtime_app;
GRANT SELECT,INSERT ON application_versions,application_invocations,application_events TO runtime_app;
GRANT SELECT,INSERT,UPDATE,DELETE ON application_records,application_unique_values,application_edges,application_record_assets TO runtime_app;
GRANT USAGE ON ALL SEQUENCES IN SCHEMA public TO runtime_app;
INSERT INTO schema_migrations(version) VALUES(3);
COMMIT;
