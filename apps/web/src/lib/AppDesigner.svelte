<script lang="ts">
 import {untrack} from 'svelte';
 import {emptyApplication,fieldTypes,keyFor,type AppSpec,type Field} from './application';
 let {initial,onsave,busy=false}:{initial?:AppSpec;onsave:(spec:AppSpec)=>void;busy?:boolean}=$props();
 let spec=$state<AppSpec>(untrack(()=>structuredClone(initial ? $state.snapshot(initial) : emptyApplication('Untitled application','untitled_application'))));
 let raw=$state(''),advanced=$state(false),error=$state('');
 function rawMode(){raw=JSON.stringify(spec,null,2);advanced=true;}
 function applyRaw(){try{const value=JSON.parse(raw);if(!value||typeof value.name!=='string'||!Array.isArray(value.roles)||!Array.isArray(value.entities)||!Array.isArray(value.pages)||!Array.isArray(value.actions)||value.entities.some((e:unknown)=>!e||typeof e!=='object'||!Array.isArray((e as {fields?:unknown}).fields)))throw Error();spec=value;advanced=false;error='';}catch{error='Use a complete application definition with roles, entities, pages, and actions. The server validates it before publication.';}}
 function addEntity(){const key=`collection_${spec.entities.length+1}`;spec.entities.push({key,label:'New collection',fields:[{key:'name',label:'Name',type:'text',required:true}],access:{read:['owner','editor','viewer'].filter(r=>spec.roles.includes(r)),write:['owner','editor'].filter(r=>spec.roles.includes(r))}});spec.pages.push({key,title:'New collection',entity:key,actions:[]});}
 function renameEntity(index:number,label:string){const entity=spec.entities[index],old=entity.key;entity.label=label;if(initial)return;const key=keyFor(label);entity.key=key;for(const page of spec.pages)if(page.entity===old){page.entity=key;page.key=key;page.title=label;}for(const action of spec.actions)if(action.entity===old)action.entity=key;for(const e of spec.entities)for(const f of e.fields)if(f.entity===old)f.entity=key;}
 function newField(index:number){const fields=spec.entities[index].fields;fields.push({key:`field_${fields.length+1}`,label:'New field',type:'text'});}
 function changeType(field:Field,type:Field['type']){field.type=type;delete field.options;delete field.entity;if(type==='select')field.options=['Option one','Option two'];if(type==='reference')field.entity=spec.entities[0].key;}
 function addCapability(){const key=`action_${spec.actions.length+1}`;spec.actions.push({key,label:'New action',entity:spec.entities[0].key,capability:'custom.ability',version:1,contract:'record-in/result-out@1'});spec.pages[0].actions.push(key);}
 function submit(){try{const value=advanced?JSON.parse(raw):spec;onsave(value);}catch{error='Invalid application JSON.';}}
</script>
<div class="designer">
 <div class="split-heading"><div><h2>{initial?'Edit application':'Design an application'}</h2><p>Define its data, pages, and capability ports. The engine creates the interface and storage.</p></div><button class="secondary" onclick={advanced?applyRaw:rawMode}>{advanced?'Use visual designer':'Application JSON'}</button></div>
 <p class="notice">Natural-language generation is not connected yet. This designer and the JSON compiler are live application-engine inputs, not a catalogue of predefined products.</p>
 {#if error}<p class="error" role="alert">{error}</p>{/if}
 {#if advanced}<label>Application specification<textarea class="code" bind:value={raw} rows="24" spellcheck="false"></textarea></label>
 {:else}
 <div class="two-col"><label>Application name<input value={spec.name} oninput={e=>{spec.name=e.currentTarget.value;if(!initial)spec.key=keyFor(spec.name);}}/></label><label>Application key<input bind:value={spec.key} disabled={!!initial}/></label></div>
 {#each spec.entities as entity,index}
 <section class="entity-design"><label>Collection<input value={entity.label} oninput={e=>renameEntity(index,e.currentTarget.value)}/></label><small>Stable key: {entity.key}</small>
 {#each entity.fields as field}
 <div class="field-design"><label>Label<input value={field.label} oninput={e=>{field.label=e.currentTarget.value;if(!initial)field.key=keyFor(field.label);}}/></label><label>Key<input bind:value={field.key} disabled={!!initial}/></label><label>Type<select value={field.type} onchange={e=>changeType(field,e.currentTarget.value as Field['type'])}>{#each fieldTypes as type}<option value={type}>{type}</option>{/each}</select></label><label class="check"><input type="checkbox" bind:checked={field.required}/>Required</label></div>
 {#if field.type==='select'}<label>Options, separated by commas<input value={field.options?.join(', ')} onchange={e=>field.options=e.currentTarget.value.split(',').map(s=>s.trim()).filter(Boolean)}/></label>{/if}
 {#if field.type==='reference'}<label>Related collection<select bind:value={field.entity}>{#each spec.entities as target}<option value={target.key}>{target.label}</option>{/each}</select></label>{/if}
 {/each}
 <button class="secondary" onclick={()=>newField(index)}>Add field</button></section>
 {/each}
 <button class="secondary" onclick={addEntity}>Add collection and page</button>
 <section class="entity-design"><div class="split-heading"><h3>Capability ports</h3><button class="secondary" onclick={addCapability}>Add capability</button></div><p>An unavailable ability does not prevent saving the app or using its data and files.</p>
 {#each spec.actions as action}<div class="field-design"><label>Button label<input bind:value={action.label}/></label><label>Capability identifier<input bind:value={action.capability}/></label><label>Version<input type="number" min="1" bind:value={action.version}/></label></div>{/each}
 </section>
 {/if}
 <div class="designer-footer"><small>Schema changes preserve existing records. Destructive changes require a separate migration.</small><button class="primary" onclick={submit} disabled={busy}>{initial?'Save new version':'Create application'}</button></div>
</div>
