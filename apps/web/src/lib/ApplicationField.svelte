<script lang="ts">
 import type {Field,Scalar,AppRecord,Asset} from './application-types';
 let {field,value=$bindable<Scalar>(null),prefix,records=[],assets=[]}:{field:Field;value:Scalar;prefix:string;records?:AppRecord[];assets?:Asset[]}=$props();
 const id=$derived(`${prefix}-${field.id}`);
 function assign(text:string){value=text===''&&!field.required?null:text;}
 function caption(record:AppRecord){const first=Object.values(record.values).find(v=>typeof v==='string'&&v.length>0);return typeof first==='string'?first.slice(0,80):record.id;}
</script>
<div class="app-field">
 <label for={id}>{field.label}{#if field.required}<span aria-label="required"> *</span>{/if}</label>
 {#if field.type==='boolean'}<input id={id} type="checkbox" checked={value===true} onchange={e=>value=e.currentTarget.checked}/>
 {:else if field.type==='choice'}<select id={id} value={typeof value==='string'?value:''} onchange={e=>assign(e.currentTarget.value)} required={field.required}><option value="">Choose…</option>{#each field.options??[] as option}<option value={option}>{option}</option>{/each}</select>
 {:else if field.type==='reference'}<select id={id} value={typeof value==='string'?value:''} onchange={e=>assign(e.currentTarget.value)} required={field.required}><option value="">Choose a record…</option>{#each records as record}<option value={record.id}>{caption(record)}</option>{/each}</select>
 {:else if field.type==='file'}<select id={id} value={typeof value==='string'?value:''} onchange={e=>assign(e.currentTarget.value)} required={field.required}><option value="">Choose an uploaded file…</option>{#each assets.filter(a=>a.state==='ready') as asset}<option value={asset.id}>{asset.filename}</option>{/each}</select>
 {:else if field.type==='text'&&(field.max_length??4096)>500}<textarea id={id} value={typeof value==='string'?value:''} oninput={e=>assign(e.currentTarget.value)} rows="4" maxlength={field.max_length??4096} required={field.required}></textarea>
 {:else}<input id={id} type={field.type==='date'?'date':'text'} inputmode={field.type==='integer'?'numeric':'text'} value={typeof value==='string'?value:''} oninput={e=>assign(e.currentTarget.value)} maxlength={field.max_length??4096} required={field.required}/>{/if}
</div>
