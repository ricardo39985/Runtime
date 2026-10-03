import Fastify from 'fastify';
import {createHash,timingSafeEqual} from 'node:crypto';
import {conservativeFallback} from './jev.ts';
import {compose,type CompositionRequest} from './composition.ts';
const token=process.env.BRIDGE_TOKEN??'';
if(token.length<32)throw new Error('A random BRIDGE_TOKEN is required.');
if((process.env.APP_ENV??'development')!=='development')throw new Error('Production identity and dispatch certification are not complete.');
const enabled=process.env.PLANNER_ENABLED==='true',model=process.env.PLANNER_MODEL??'',apiKey=process.env.OPENAI_API_KEY??'';
const app=Fastify({bodyLimit:384*1024,logger:{level:'warn',redact:['req.headers.authorization','req.body','res.body']}});
const digest=(v:string)=>createHash('sha256').update(v).digest();
function authorized(header:string|undefined){return timingSafeEqual(digest(header??''),digest(`Bearer ${token}`));}
app.get('/health/live',async()=>({status:'ok',version:'0.2.0',composition_enabled:enabled&&!!model&&!!apiKey,routing_mode:'fixture'}));
app.post('/internal/v1/route',async(request,reply)=>{if(!authorized(request.headers.authorization))return reply.code(401).send({error:'unauthorized'});return {...conservativeFallback('routing_evaluation_not_enabled'),source:'fixture'};});
const dispatched=new Set<string>();
app.post('/internal/v1/compose',{schema:{body:{type:'object',additionalProperties:false,required:['model','brief','schema','max_output_tokens','dispatch_id'],properties:{model:{type:'string',maxLength:160},brief:{type:'string',minLength:1,maxLength:12000},schema:{type:'object'},previous_spec:{type:'object'},max_output_tokens:{const:8192},dispatch_id:{type:'string',pattern:'^[a-f0-9]{32}$'},base_revision:{type:'integer'},application_id:{type:'string'}}}}},async(request,reply)=>{
  if(!authorized(request.headers.authorization))return reply.code(401).send({error:'unauthorized'});
  const body=request.body as CompositionRequest;
  if(!enabled||!model||!apiKey)return reply.code(503).send({error:'planner_not_configured',provider_dispatched:false});
  if(body.model!==model)return reply.code(403).send({error:'model_not_authorized',provider_dispatched:false});
  // The core's persisted dispatch ledger is authoritative across restarts. This
  // secondary guard rejects duplicate transport requests during this process.
  if(dispatched.has(body.dispatch_id)||dispatched.size>=10000)return reply.code(409).send({error:'duplicate_or_capacity',provider_dispatched:false});
  dispatched.add(body.dispatch_id);
  try{return await compose(body,apiKey);}catch{return reply.code(502).send({error:'provider_result_unknown',provider_dispatched:true});}
});
for(const signal of ['SIGINT','SIGTERM'] as const)process.on(signal,()=>{void app.close();});
await app.listen({port:Number(process.env.PORT??8081),host:process.env.BIND_ADDRESS??'127.0.0.1'});
