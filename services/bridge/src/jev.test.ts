import test from 'node:test';
import assert from 'node:assert/strict';
import { classifyWithJev, jevRequest, parseJev } from './jev.ts';
const answer = () => ({ model: 'jev-1.13.0', answers: { category: {
  type: 'choice', choice: 'routine', confidence: 0.93,
  probabilities: { routine: 0.97, planner: 0.02, clarify: 0.01, unsupported: 0 }
}}, usage: { input_tokens: 200, output_tokens: 8 } });
test('uses the documented System One shape and pinned model', () => {
  const request = jevRequest('Summarize this customer note.');
  assert.equal(request.model, 'jev-1.13.0'); assert.equal(request.questions.category.type, 'choice');
  assert.equal(request.state.task, 'Summarize this customer note.');
});
test('rejects empty input', () => assert.throws(() => jevRequest(' ')));
test('bounds input', () => assert.throws(() => jevRequest('x'.repeat(4001))));
test('a cheap recommendation cannot bypass evaluation gate', () => {
  const result = parseJev(answer()); assert.equal(result.candidate, 'routine'); assert.equal(result.route, 'planner');
});
test('rejects an unexpected model version', () => { const data = answer(); data.model = 'jev-latest'; assert.throws(() => parseJev(data)); });
test('rejects unknown route', () => { const data = answer(); data.answers.category.choice = 'execute_shell'; assert.throws(() => parseJev(data)); });
test('rejects NaN confidence', () => { const data = answer(); data.answers.category.confidence = NaN; assert.throws(() => parseJev(data)); });
test('rejects invalid probabilities', () => { const data = answer(); data.answers.category.probabilities.routine = 0.5; assert.throws(() => parseJev(data)); });
test('rejects invalid usage', () => { const data = answer(); data.usage.input_tokens = -1; assert.throws(() => parseJev(data)); });
test('missing key makes no network request', async () => {
  const transport: typeof fetch = async () => { throw new Error('Must not call'); };
  assert.equal((await classifyWithJev('plan a workflow', '', transport)).reason, 'credential_missing');
});
test('rate limit falls back without retry amplification', async () => {
  let calls = 0;
  const transport: typeof fetch = async () => { calls++; return new Response('', {status: 429}); };
  assert.equal((await classifyWithJev('plan a workflow', 'test-only', transport)).route, 'planner'); assert.equal(calls, 1);
});
test('transport errors preserve uncertainty about usage', async () => {
  const transport: typeof fetch = async () => { throw new Error('timeout'); };
  const result = await classifyWithJev('plan a workflow', 'test-only', transport);
  assert.equal(result.reason, 'provider_failure_usage_unknown'); assert.equal(result.inputTokens, null);
});
test('successful adapter dispatches only to the fixed provider endpoint', async () => {
  const transport: typeof fetch = async (url, options) => {
    assert.equal(url, 'https://api.typesafe.ai/v1/systemone'); assert.equal(options?.redirect, 'error');
    return new Response(JSON.stringify(answer()), {status: 200});
  };
  assert.equal((await classifyWithJev('plan a workflow', 'test-only', transport)).source, 'jev');
});
