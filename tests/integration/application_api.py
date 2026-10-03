"""Real HTTP + PostgreSQL + object-store tests. No external model calls or sends."""
import concurrent.futures
import copy
import hashlib
import json
import os
from pathlib import Path
import sys
import urllib.error
import urllib.request
import uuid
BASE=os.getenv('BASE_URL','http://localhost:8080')
TOKEN=os.environ['DEV_API_TOKEN']
READER=os.environ.get('DEV_READER_TOKEN','')
ROOT=Path(__file__).resolve().parents[2]
count=0

def call(path,method='GET',body=None,key=None,token=TOKEN,raw=False):
    headers={'Authorization':'Bearer '+token}
    if body is not None: headers['Content-Type']='application/octet-stream' if raw else 'application/json'
    if key: headers['Idempotency-Key']=key
    payload=body if raw else (None if body is None else json.dumps(body).encode())
    request=urllib.request.Request(BASE+'/api/v1/studio/'+path,data=payload,headers=headers,method=method)
    try:
        with urllib.request.urlopen(request,timeout=30) as response:
            return response.status, response.read() if 'application/json' not in response.headers.get('Content-Type','') else json.load(response)
    except urllib.error.HTTPError as response:
        return response.code,json.load(response)

def check(value,label):
    global count
    if not value: raise AssertionError(label)
    count+=1;print('PASS',label,flush=True)

def accepted(result,label,codes=(200,201)):
    status,value=result
    check(status in codes,label+': '+str(status)+' '+(str(value)[:160] if status not in codes else ''))
    return value

def create(spec):
    return accepted(call('apps','POST',spec,uuid.uuid4().hex),'create arbitrary application')

def endpoint(app,space=None): return f'apps/{app["id"]}/spaces/{space or app["spaces"][0]["id"]}'

state_path=ROOT/'test-results/application-persistence.json'
if '--after-restart' in sys.argv:
    state=json.loads(state_path.read_text())
    app=accepted(call('apps/'+state['app']),'application survives core restart')
    check(app['revision']==2,'schema version survives restart')
    row=accepted(call(state['base']+'/records/projects/'+state['record']),'record survives restart')
    check(row['values']['name']=='A persistent painting project','stored record is unchanged')
    data=accepted(call(state['base']+'/assets/'+state['asset']+'/content'),'asset survives restart')
    check(hashlib.sha256(data).hexdigest()==state['sha256'],'decompressed bytes match after restart')
    print(f'{count} restart assertions passed');sys.exit(0)

check(call('meta',token='wrong')[0]==401,'invalid identity rejected')
check(call('missing')[0]==404,'unknown endpoint is not a fake application')
check(call('generation/config')[1]['configured'] is False,'unconfigured planner explicitly reported')
check(call('generation','POST',{'brief':'Build an unrelated SaaS'},uuid.uuid4().hex)[0]==503,'no template substituted for missing planner')
creative=json.loads((ROOT/'examples/creative-studio.json').read_text())
review=accepted(call('compile','POST',creative),'compile an app with missing paint')
check(review['capabilities'][0]['status']=='missing','missing capability remains a declared dependency')
key=uuid.uuid4().hex
app=accepted(call('apps','POST',creative,key),'publish creative studio')
check(call('apps','POST',creative,key)[1]['id']==app['id'],'application creation is idempotent')
check(call('apps','POST',dict(creative,name='Different'),key)[0]==409,'application idempotency conflict rejected')
b=endpoint(app)
row_key=uuid.uuid4().hex
values={'name':'A persistent painting project','brief':'Paint an evening landscape.'}
row=accepted(call(b+'/records/projects','POST',{'values':values},row_key),'create a project without paint implementation')
check(call(b+'/records/projects','POST',{'values':values},row_key)[1]['id']==row['id'],'record creation is idempotent')
check(row['values']['status']=='draft','declared default applied by core')
check(call(b+'/records/projects','POST',{'values':dict(values,authority='owner')},uuid.uuid4().hex)[0]==400,'undeclared fields rejected')
check(call(b+'/records/projects','POST',{'values':{'brief':'Missing name'}},uuid.uuid4().hex)[0]==400,'required fields enforced')
check(call(b+'/actions/paint/invoke','POST',{'record_id':row['id'],'expected_version':row['version']},uuid.uuid4().hex)[0]==424,'only the unavailable paint action is blocked')
check(call(b+'/records/projects')[1]['items'][0]['id']==row['id'],'missing ability does not hide persistent data')

upgrade=copy.deepcopy(creative)
upgrade['entities'][0]['fields'] += [
 {'id':'priority','label':'Priority','type':'choice','options':['normal','urgent'],'required':True,'default':'normal'},
 {'id':'client','label':'Client','type':'reference','target':'clients'}]
upgrade['entities'].append({'id':'clients','label':'Clients','fields':[{'id':'name','label':'Client name','type':'text','required':True},{'id':'code','label':'Code','type':'text','required':True,'unique':True,'max_length':64}],'read_roles':['owner','reader'],'write_roles':['owner']})
upgrade['pages'].append({'id':'clients','title':'Clients','blocks':[{'kind':'table','entity':'clients'},{'kind':'form','entity':'clients'}]})
upgraded=accepted(call('apps/'+app['id']+'/revisions','POST',{'expected_revision':1,'spec':upgrade}),'publish additive schema revision')
check(upgraded['revision']==2,'immutable revision advances')
check(call('apps/'+app['id']+'/revisions','POST',{'expected_revision':1,'spec':upgrade})[0]==409,'stale schema editor rejected')
row=accepted(call(b+'/records/projects/'+row['id']),'read existing record through updated schema')
check(row['values']['name']==values['name'] and row['values']['priority']=='normal','safe default backfill preserves existing data')
lossy=copy.deepcopy(upgrade);lossy['entities'][0]['fields']=[f for f in lossy['entities'][0]['fields'] if f['id']!='priority']
check(call('apps/'+app['id']+'/revisions','POST',{'expected_revision':2,'spec':lossy})[0]==409,'destructive schema edit requires migration')
client=accepted(call(b+'/records/clients','POST',{'values':{'name':'Customer A','code':'a-001'}},uuid.uuid4().hex),'create related entity')
check(call(b+'/records/clients','POST',{'values':{'name':'Duplicate','code':'a-001'}},uuid.uuid4().hex)[0]==409,'generic uniqueness enforced')
row=accepted(call(b+'/records/projects/'+row['id'],'PATCH',{'expected_version':row['version'],'values':{'client':client['id']}}),'store a validated relationship')
check(call(b+'/records/clients/'+client['id']+'/delete','POST',{'expected_version':client['version']})[0]==409,'referenced records cannot be silently deleted')
space_b=accepted(call('apps/'+app['id']+'/spaces','POST',{'name':'Another customer'}),'create isolated customer data space')['id']
b2=endpoint(app,space_b)
check(call(b2+'/records/projects')[1]['items']==[],'new customer starts with an isolated dataset')
check(call(b2+'/records/projects','POST',{'values':dict(values,client=client['id'])},uuid.uuid4().hex)[0]==400,'cross-space relationship rejected')

if not READER: raise AssertionError('DEV_READER_TOKEN must be configured for isolation tests')
check(call('apps/'+app['id'],token=READER)[0]==404,'ungranted reader cannot discover application')
accepted(call(b+'/members','POST',{'principal_id':'development-reader','role':'reader'}),'grant explicit read-only membership')
check(call(b+'/records/projects',token=READER)[0]==200,'declared reader can read records')
check(call(b+'/records/projects','POST',{'values':values},uuid.uuid4().hex,token=READER)[0]==403,'reader cannot create records')
check(call(b+'/members','POST',{'principal_id':'development-reader','role':'owner'},token=READER)[0]==403,'reader cannot elevate their role')
check(call(b2+'/records/projects',token=READER)[0]==404,'membership does not leak into another customer space')
check(call(b+'/assets',token=READER)[0]==403,'storage permissions are separate from record-read access')

payload=b'Runtime lossless asset storage\n'*4096
upload_key=uuid.uuid4().hex
asset=accepted(call(b+'/assets/upload?filename=project-notes.txt','POST',payload,upload_key,raw=True),'upload through real compression and storage pipeline')
check(asset['codec']=='zstd' and int(asset['stored_size'])<len(payload),'compressible object stored more compactly')
check(call(b+'/assets/upload?filename=project-notes.txt','POST',payload,upload_key,raw=True)[1]['id']==asset['id'],'retry reuses the same immutable object')
check(call(b+'/assets/upload?filename=project-notes.txt','POST',b'different',upload_key,raw=True)[0]==409,'same upload key cannot replace different content')
download=accepted(call(b+'/assets/'+asset['id']+'/content'),'authorized read decompresses verified original bytes')
check(download==payload,'compression is byte-for-byte lossless')
check(call(b2+'/assets/'+asset['id']+'/content')[0]==404,'opaque asset ID does not grant cross-space access')
check(call(b+'/assets/upload?filename=reader.txt','POST',b'x',uuid.uuid4().hex,token=READER,raw=True)[0]==403,'reader cannot force an unauthorized upload')
row=accepted(call(b+'/records/projects/'+row['id'],'PATCH',{'expected_version':row['version'],'values':{'artwork':asset['id']}}),'link asset using the generic file field')
check(call(b2+'/records/projects','POST',{'values':dict(values,artwork=asset['id'])},uuid.uuid4().hex)[0]==400,'cross-space file link rejected')
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    results=list(pool.map(lambda text:call(b+'/records/projects/'+row['id'],'PATCH',{'expected_version':row['version'],'values':{'brief':text}}),['Concurrent A','Concurrent B']))
check(sorted(status for status,_ in results)==[200,409],'concurrent edits cannot overwrite each other silently')

editorial=json.loads((ROOT/'examples/editorial-desk.json').read_text())
other=create(editorial);other_base=endpoint(other)
article=accepted(call(other_base+'/records/articles','POST',{'values':{'title':'Independent model','copy':'one two three four'}},uuid.uuid4().hex),'create unrelated application record')
invocation_key=uuid.uuid4().hex
arguments={'record_id':article['id'],'expected_version':article['version']}
result=accepted(call(other_base+'/actions/count/invoke','POST',arguments,invocation_key),'execute a real installed capability')
check(result['values']['word_count']=='4','capability result validated and persisted')
check(call(other_base+'/actions/count/invoke','POST',arguments,invocation_key)[1]==result,'repeated action request returns its persisted receipt')
check(call(other_base+'/records/articles/'+row['id'])[0]==404,'record IDs are scoped to their application and entity')

for i in range(10):
    variant=copy.deepcopy(editorial);eid=f'collection_{i}';title=f'heading_{i}';body=f'body_{i}';total=f'total_{i}'
    variant['name']=f'Unseen composition {i}';variant['entities'][0]['id']=eid
    for f,new in zip(variant['entities'][0]['fields'],[title,body,total]):f['id']=new
    for block in variant['pages'][0]['blocks']:
        if 'entity' in block:block['entity']=eid
    variant['actions'][0].update(entity=eid,inputs={'text':body},outputs={total:'words'})
    generated=create(variant);root=endpoint(generated)
    created=accepted(call(root+'/records/'+eid,'POST',{'values':{title:f'Item {i}',body:'arbitrary structure'}},uuid.uuid4().hex),'unseen entity and field bindings work')
    applied=accepted(call(root+'/actions/count/invoke','POST',{'record_id':created['id'],'expected_version':created['version']},uuid.uuid4().hex),'unseen action bindings work')
    check(applied['values'][total]=='2','no application-name or entity-name dispatch table')

state_path.parent.mkdir(exist_ok=True)
state_path.write_text(json.dumps({'app':app['id'],'base':b,'record':row['id'],'asset':asset['id'],'sha256':hashlib.sha256(payload).hexdigest()}))
print(f'{count} actual-stack assertions passed; live model calls: 0; external sends: 0')
