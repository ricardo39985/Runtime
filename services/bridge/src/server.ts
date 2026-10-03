import Fastify from 'fastify';
import { createHash, timingSafeEqual } from 'node:crypto';
import { conservativeFallback } from './jev.ts';
const token = process.env.BRIDGE_TOKEN ?? '';
if (token.length < 32) throw new Error('A random BRIDGE_TOKEN of at least 32 characters is required.');
if ((process.env.APP_ENV ?? 'development') !== 'development' || (process.env.PROVIDER_MODE ?? 'fixture') !== 'fixture') {
  throw new Error('This bridge milestone is fixture-only; production/live mode is disabled.');
}
const app = Fastify({ bodyLimit: 8192, logger: { level: 'warn', redact: ['req.headers.authorization', 'req.body', 'res.body'] } });
app.get('/health/live', async () => ({ status: 'ok', mode: 'fixture', version: '0.1.0' }));
app.post('/internal/v1/route', { schema: { body: {
  type: 'object', additionalProperties: false, required: ['state'], properties: {state: {type: 'string', minLength: 1, maxLength: 4000}}
}}}, async (request, reply) => {
  const digest = (value: string) => createHash('sha256').update(value).digest();
  if (!timingSafeEqual(digest(request.headers.authorization ?? ''), digest(`Bearer ${token}`))) return reply.code(401).send({error: 'unauthorized'});
  return { ...conservativeFallback('fixture_mode_no_provider_call'), source: 'fixture' };
});
for (const signal of ['SIGINT', 'SIGTERM'] as const) process.on(signal, () => { void app.close(); });
await app.listen({ port: Number(process.env.PORT ?? 8081), host: process.env.BIND_ADDRESS ?? '127.0.0.1' });
