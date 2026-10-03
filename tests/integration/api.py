"""Exercises the real local HTTP/PostgreSQL stack; no external providers."""
import concurrent.futures
import json
import os
import time
import urllib.error
import urllib.request
import uuid

BASE = os.environ.get('BASE_URL', 'http://localhost:8080')
TOKEN = os.environ['DEV_API_TOKEN']
COUNT = 0

def call(path, method='GET', body=None, key=None, token=TOKEN):
    headers = {'Authorization': 'Bearer ' + token}
    if body is not None:
        headers['Content-Type'] = 'application/json'
    if key:
        headers['Idempotency-Key'] = key
    request = urllib.request.Request(BASE + path, data=None if body is None else json.dumps(body).encode(), headers=headers, method=method)
    try:
        with urllib.request.urlopen(request, timeout=15) as response:
            return response.status, json.load(response)
    except urllib.error.HTTPError as response:
        return response.code, json.load(response)

def check(condition, label):
    global COUNT
    if not condition:
        raise AssertionError(label)
    COUNT += 1
    print('PASS', label)

def wait_run(identifier, state):
    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        status, run = call('/api/v1/runs/' + identifier)
        if status == 200 and run['status'] == state:
            return run
        if run.get('status') == 'failed':
            raise AssertionError('Workflow failed')
        time.sleep(0.15)
    raise AssertionError('Run did not reach ' + state)

check(call('/health/ready')[0] == 200, 'readiness checks real migrated database')
check(call('/api/v1/meta', token='wrong')[0] == 401, 'invalid token rejected')
check(call('/api/v1/unknown')[0] == 404, 'unknown API route is JSON 404, not SPA')
_, sample = call('/api/v1/sample')
body = {'schema_version': 1, 'title': 'Integration follow-up', 'csv': sample['csv'], 'min_days': 14}
key = uuid.uuid4().hex
status, started = call('/api/v1/requests', 'POST', body, key)
check(status == 202, 'request accepted durably')
identifier = started['id']
check(call('/api/v1/requests', 'POST', body, key)[1]['id'] == identifier, 'same idempotency key reuses run')
check(call('/api/v1/requests', 'POST', dict(body, title='Different'), key)[0] == 409, 'idempotency conflict rejected')
run = wait_run(identifier, 'awaiting_approval')
check(len(run['drafts']) == 2 and run['excluded'] == 1, 'paid invoice excluded deterministically')
check(run['bindings']['total_USD']['amount_minor'] == '343000', 'exact total bound to UI')
old = {'version': run['version'], 'proposal_hash': run['proposal_hash'], 'snapshot_ack': True}
check(call(f'/api/v1/runs/{identifier}/approve', 'POST', dict(old, snapshot_ack=False))[0] == 400, 'snapshot acknowledgement required')
status, edited = call(f'/api/v1/runs/{identifier}/drafts', 'PATCH', {'version': run['version'], 'index': 0, 'body': 'An edited, explicitly reviewed fixture message.'})
check(status == 200 and edited['version'] == run['version'] + 1, 'edit advances version')
check(edited['proposal_hash'] != run['proposal_hash'], 'edit changes exact payload hash')
check(call(f'/api/v1/runs/{identifier}/approve', 'POST', old)[0] == 409, 'old approval rejected after edit')
approval = {'version': edited['version'], 'proposal_hash': edited['proposal_hash'], 'snapshot_ack': True}
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    results = list(pool.map(lambda _: call(f'/api/v1/runs/{identifier}/approve', 'POST', approval), range(4)))
check(all(code in (200, 202) for code, _ in results), 'concurrent identical approvals are idempotent')
completed = wait_run(identifier, 'completed')
check(len(completed['receipts']) == 2 and all(not item['external_action'] for item in completed['receipts']), 'only explicit simulated receipts created')
_, events = call(f'/api/v1/runs/{identifier}/events')
check(sum(item['kind'] == 'approved' for item in events) == 1, 'one approval event despite concurrent retries')
check(sum(item['kind'] == 'simulation_completed' for item in events) == 1, 'one simulation completion')
check(call(f'/api/v1/runs/{identifier}/events?after={events[-1]["sequence"]}')[1] == [], 'durable event cursor replay')
check(call(f'/api/v1/runs/{identifier}/cancel', 'POST', {})[0] == 409, 'completed action cannot be falsely cancelled')
check(call('/api/v1/requests', 'POST', dict(body, workspace_id='isolation-workspace'), uuid.uuid4().hex)[0] == 400, 'browser cannot provide workspace authority')
check(call('/api/v1/requests', 'POST', dict(body, csv=sample['csv'].replace('248000', '1.25')), uuid.uuid4().hex)[0] == 400, 'decimal money rejected at HTTP boundary')
_, second = call('/api/v1/requests', 'POST', body, uuid.uuid4().hex)
check(call(f'/api/v1/runs/{second["id"]}/cancel', 'POST', {})[0] == 200, 'queued or pending run can be cancelled')
print(f'{COUNT} real-stack API assertions passed; external provider calls: 0')
