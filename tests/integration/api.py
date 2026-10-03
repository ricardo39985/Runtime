"""Real HTTP/PostgreSQL acceptance; no provider/model calls or fixture API replacement."""
import concurrent.futures
import copy
import hashlib
import json
import os
from pathlib import Path
import sys
import urllib.error
import urllib.parse
import urllib.request
import uuid

BASE = os.environ.get('BASE_URL', 'http://localhost:8080')
TOKEN = os.environ['DEV_API_TOKEN']
COUNT = 0

def call(path, method='GET', body=None, key=None, raw=None, filename='asset.txt', token=TOKEN):
    headers = {'Authorization': 'Bearer ' + token}
    data = None
    if body is not None:
        headers['Content-Type'] = 'application/json'
        data = json.dumps(body).encode()
    if raw is not None:
        headers.update({'Content-Type': 'application/octet-stream', 'X-Filename': urllib.parse.quote(filename)})
        data = raw
    if key:
        headers['Idempotency-Key'] = key
    request = urllib.request.Request(BASE + path, data=data, headers=headers, method=method)
    try:
        response = urllib.request.urlopen(request, timeout=20)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        data = response.read()
        return response.status, json.loads(data) if 'application/json' in response.headers.get('Content-Type', '') else data

def check(condition, label):
    global COUNT
    if not condition:
        raise AssertionError(label)
    COUNT += 1
    print('PASS', label)

def blueprint(key, entity='projects'):
    return {
        'schema_version': 1, 'key': key, 'name': 'Independent ' + entity,
        'roles': ['owner', 'editor', 'viewer'],
        'entities': [{'key': entity, 'label': entity.title(), 'fields': [
            {'key': 'title', 'label': 'Title', 'type': 'text', 'required': True},
            {'key': 'amount', 'label': 'Amount', 'type': 'decimal'},
            {'key': 'attachment', 'label': 'Attachment', 'type': 'file'},
        ], 'access': {'read': ['owner', 'editor', 'viewer'], 'write': ['owner', 'editor']}}],
        'pages': [{'key': entity, 'title': entity.title(), 'entity': entity, 'actions': ['paint', 'snapshot']}],
        'actions': [
            {'key': 'paint', 'label': 'Paint', 'entity': entity, 'capability': 'media.paint', 'version': 1, 'contract': 'record-in/result-out@1'},
            {'key': 'snapshot', 'label': 'Snapshot', 'entity': entity, 'capability': 'core.records.snapshot', 'version': 1, 'contract': 'record-in/result-out@1'},
        ]
    }

proof_path = Path('evidence/ci/recovery.json')
if '--resume' in sys.argv:
    proof = json.loads(proof_path.read_text())
    check(call(f'/api/v1/apps/{proof["app"]}/records/projects/{proof["record"]}')[1]['data']['title'] == 'Persist through restart', 'record survives actual service restart')
    data = call(f'/api/v1/apps/{proof["app"]}/files/{proof["file"]}/content')[1]
    check(hashlib.sha256(data).hexdigest() == proof['sha256'], 'file survives restart with exact original hash')
    print(f'{COUNT} post-restart assertions passed')
    sys.exit(0)

check(call('/health/ready')[0] == 200, 'application migrations ready')
check(call('/api/v1/apps', token='wrong')[0] == 401, 'unauthenticated app access rejected')
check(call('/api/v1/requests', 'POST', {})[0] == 404, 'no invoice-specific request path in active engine')
spec = blueprint('app_' + uuid.uuid4().hex)
status, created = call('/api/v1/apps', 'POST', spec)
check(status == 201, 'arbitrary app created even without paint capability')
app = created['id']
base = f'/api/v1/apps/{app}'
check(call('/api/v1/apps', 'POST', spec)[1]['id'] == app, 'same application definition is idempotent')
status, loaded = call(base)
check(status == 200 and loaded['spec'] == spec, 'complete original AppSpec persists')
check(not loaded['ui']['pages'][0]['actions'][0]['resolution']['available'], 'only absent capability is marked unavailable')
body = {'app_version': 1, 'data': {'title': 'Persist through restart', 'amount': '9007199254740993.25'}}
key = uuid.uuid4().hex
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    results = list(pool.map(lambda _: call(base + '/records/projects', 'POST', body, key), range(4)))
check(all(code in (200, 201) for code, _ in results), 'concurrent identical creates accepted safely')
check(len({data['id'] for _, data in results}) == 1, 'concurrent idempotency creates exactly one record')
record = results[0][1]
check(record['data']['amount'] == body['data']['amount'], 'decimal amount preserves exact precision')
check(call(base + '/records/projects', 'POST', {'app_version': 1, 'data': {'title': 'Different'}}, key)[0] == 409, 'idempotency payload conflict rejected')
check(call(base + '/records/projects', 'POST', {'app_version': 1, 'data': {'title': 'x', 'sql': 'DROP'}}, uuid.uuid4().hex)[0] == 400, 'unknown record field rejected')
invoke = {'app_version': 1, 'record_id': record['id'], 'record_version': 1}
check(call(base + '/actions/paint', 'POST', invoke)[0] == 424, 'missing capability fails explicitly without fake success')
check(call(base + '/actions/snapshot', 'POST', invoke)[0] == 200, 'installed local capability executes through the same port')
check(call(base + '/records/projects/' + record['id'])[1]['data'] == body['data'], 'blocked painting never destroys app data')
asset = (b'Lossless application asset\n' * 2048) + bytes(range(256))
upload_key = uuid.uuid4().hex
status, file = call(base + '/files', 'POST', key=upload_key, raw=asset)
check(status == 201 and file['codec'] == 'zstd' and file['stored_size'] < file['original_size'], 'compression pipeline stores genuinely smaller bytes')
check(call(base + '/files', 'POST', key=upload_key, raw=asset)[1]['id'] == file['id'], 'upload retries reuse the same file')
check(call(base + '/files', 'POST', key=upload_key, raw=b'changed')[0] == 409, 'upload key cannot silently change content')
check(call(base + '/files/' + file['id'] + '/content')[1] == asset, 'download restores exact bytes including binary data')
check(file['sha256'] == hashlib.sha256(asset).hexdigest(), 'server integrity digest matches original bytes')
status, second_file = call(base + '/files', 'POST', key=uuid.uuid4().hex, raw=asset, filename='other-name.bin')
check(status == 201 and second_file['id'] != file['id'] and second_file['sha256'] == file['sha256'], 'file metadata independent of deduplicated content')
check(call(base + '/files', 'POST', key=uuid.uuid4().hex, raw=b'x', filename='../escape')[0] == 400, 'path-like filename rejected')
foreign = blueprint('app_' + uuid.uuid4().hex, 'observations')
status, other = call('/api/v1/apps', 'POST', foreign)
check(status == 201, 'different entity/application uses unchanged engine')
check(call(f'/api/v1/apps/{other["id"]}/files/{file["id"]}/content')[0] == 404, 'file cannot be fetched through another application')
check(call(f'/api/v1/apps/{other["id"]}/records/observations', 'POST', {'app_version': 1, 'data': {'title': 'bad link', 'attachment': file['id']}}, uuid.uuid4().hex)[0] == 400, 'cross-application file link rejected')
new_spec = copy.deepcopy(spec)
new_spec['entities'][0]['fields'].append({'key': 'note', 'label': 'Note', 'type': 'multiline'})
status, upgraded = call(base, 'PATCH', {'version': 1, 'spec': new_spec})
check(status == 200 and upgraded['version'] == 2, 'non-destructive schema extension publishes a new version')
check(call(base + '/records/projects/' + record['id'])[1]['data'] == body['data'], 'schema extension preserves existing records')
check(call(base, 'PATCH', {'version': 1, 'spec': new_spec})[0] == 409, 'stale application edit rejected')
bad_spec = copy.deepcopy(new_spec)
bad_spec['entities'][0]['fields'].pop(0)
check(call(base, 'PATCH', {'version': 2, 'spec': bad_spec})[0] == 409, 'destructive schema edit requires migration')
check(call(base + '/records/projects', 'POST', body, uuid.uuid4().hex)[0] == 409, 'stale form cannot write after schema change')
changes = [{'app_version': 2, 'version': 1, 'data': {**body['data'], 'note': str(index)}} for index in range(2)]
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    result = list(pool.map(lambda payload: call(base + '/records/projects/' + record['id'], 'PATCH', payload), changes))
check(sorted(code for code, _ in result) == [200, 409], 'competing record edits cannot silently overwrite')
# A relationship added at runtime enforces the generic referential path.
new_spec['entities'][0]['fields'].append({'key': 'parent', 'label': 'Parent', 'type': 'reference', 'entity': 'projects'})
check(call(base, 'PATCH', {'version': 2, 'spec': new_spec})[0] == 200, 'relationship field is not hardcoded')
status, child = call(base + '/records/projects', 'POST', {'app_version': 3, 'data': {'title': 'Child', 'parent': record['id'], 'attachment': file['id']}}, uuid.uuid4().hex)
check(status == 201, 'generic record and file relationships persist')
check(call(base + '/records/projects/' + record['id'] + '/archive', 'POST', {'version': 2})[0] == 409, 'referenced record cannot be orphaned')
check(call(base + '/records/projects/' + child['id'] + '/archive', 'POST', {'version': 1})[0] == 200, 'unreferenced record can be archived')
_, history = call(base + '/events')
check(any(e['kind'] == 'capability_blocked' for e in history), 'missing-capability event commits despite HTTP424')
check(call(base + '/events?after=' + history[-1]['sequence'])[1] == [], 'persistent events support reconnect cursor')
proof_path.parent.mkdir(parents=True, exist_ok=True)
proof_path.write_text(json.dumps({'app': app, 'record': record['id'], 'file': file['id'], 'sha256': file['sha256']}))
print(f'{COUNT} actual-stack application assertions passed; provider calls: 0')
