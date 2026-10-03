-- The application needs UPDATE to claim jobs with SELECT FOR UPDATE.
BEGIN;
SELECT pg_advisory_xact_lock(740013001);
GRANT UPDATE ON jobs TO runtime_app;
INSERT INTO schema_migrations(version) VALUES (2) ON CONFLICT DO NOTHING;
COMMIT;
