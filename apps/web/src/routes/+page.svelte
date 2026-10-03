<script lang="ts">
 import AppDesigner from '$lib/AppDesigner.svelte';
 import AppWorkspace from '$lib/AppWorkspace.svelte';
 import type {AppSpec,AppDocument,Asset} from '$lib/application';
 import '../application.css';
 let token=$state(''),authenticated=$state(false),busy=$state(false),error=$state(''),mode=$state<'apps'|'create'|'edit'|'workspace'>('apps');
 const uploadKeys=new Map<string,string>();
 let apps=$state<{id:string;name:string;key:string;version:number}[]>([]),current=$state<AppDocument|null>(null);
 async function api<T>(path:string,method='GET',body?:unknown,key?:string):Promise<T>{
  const headers:Record<string,string>={authorization:`Bearer ${token}`};if(body!==undefined)headers['content-type']='application/json';if(key)headers['idempotency-key']=key;
  const response=await fetch(`/api/v1/${path}`,{method,headers,body:body===undefined?undefined:JSON.stringify(body),signal:AbortSignal.timeout(15000)});
  const result=await response.json();if(!response.ok)throw Error(result.error?.message??`Request failed (${response.status}).`);return result;
 }
 async function task(fn:()=>Promise<void>){if(busy)return;busy=true;error='';try{await fn();}catch(e){error=e instanceof Error?e.message:'Operation failed.';}finally{busy=false;}}
 async function list(){apps=await api('apps');}
 function login(){void task(async()=>{await api('meta');await list();authenticated=true;});}
 async function open(id:string){current=await api<AppDocument>(`apps/${id}`);mode='workspace';}
 function save(spec:AppSpec){void task(async()=>{const result=mode==='edit'&&current?await api<{id:string}>(`apps/${current.id}`,'PATCH',{version:current.version,spec}):await api<{id:string}>('apps','POST',spec);await list();await open(result.id);});}
 async function upload(file:File):Promise<Asset>{
  if(!current)throw Error('Open an application first.');if(file.size>8*1024*1024)throw Error('File exceeds the 8 MiB limit.');
  const digest=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',await file.arrayBuffer()))).map(v=>v.toString(16).padStart(2,'0')).join('');const fingerprint=JSON.stringify([current.id,digest,file.name,file.type]);if(!uploadKeys.has(fingerprint))uploadKeys.set(fingerprint,crypto.randomUUID());
  const headers:Record<string,string>={authorization:`Bearer ${token}`,'Idempotency-Key':uploadKeys.get(fingerprint)!,'Content-Type':file.type||'application/octet-stream','X-Filename':encodeURIComponent(file.name)};
  const response=await fetch(`/api/v1/apps/${current.id}/files`,{method:'POST',headers,body:file,signal:AbortSignal.timeout(30000)});const result=await response.json();if(!response.ok)throw Error(result.error?.message??'Upload failed.');uploadKeys.delete(fingerprint);return result;
 }
 async function download(file:Asset){if(!current)return;const response=await fetch(`/api/v1/apps/${current.id}/files/${file.id}/content`,{headers:{authorization:`Bearer ${token}`},signal:AbortSignal.timeout(30000)});if(!response.ok){const result=await response.json();throw Error(result.error?.message??'Download failed.');}const blob=await response.blob();const url=URL.createObjectURL(blob);const anchor=document.createElement('a');anchor.href=url;anchor.download=file.filename;anchor.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}
 function logout(){uploadKeys.clear();token='';authenticated=false;current=null;apps=[];mode='apps';}
</script>
<svelte:head><title>Runtime — Application engine</title><meta name="description" content="Build applications from data, pages, and extensible capabilities."/></svelte:head>
<header class="shell-header"><a href="/" class="brand"><span>r_</span>Runtime<small>APPLICATION ENGINE</small></a><span class="local-badge">LOCAL DEVELOPMENT</span>{#if authenticated}<button onclick={logout}>Lock workspace</button>{/if}</header>
<main>
{#if error}<div class="error" role="alert">{error}<button aria-label="Dismiss error" onclick={()=>error=''}>×</button></div>{/if}
{#if !authenticated}
 <section class="welcome"><p class="eyebrow">SOFTWARE FROM COMPOSABLE CAPABILITIES</p><h1>The application is yours.<br/>The foundations are shared.</h1><p>Pages, records, relationships, and file storage belong to the platform. Specialized abilities can be installed without rebuilding the application around them.</p>
 <form class="panel login" onsubmit={e=>{e.preventDefault();login();}}><h2>Open your development workspace</h2><label>Development access token<input type="password" bind:value={token} autocomplete="off" minlength="32" required/></label><button class="primary" disabled={busy}>Open workspace</button><small>Use DEV_API_TOKEN from your local .env. Public sign-in and the natural-language planner are not connected yet.</small></form></section>
{:else}
 <nav class="breadcrumbs"><button onclick={()=>void task(async()=>{await list();mode='apps';})}>Applications</button>{#if mode!=='apps'}<span>/</span><span>{mode==='create'?'New application':current?.spec.name}</span>{/if}</nav>
 {#if mode==='apps'}
 <div class="page-heading"><div><p class="eyebrow">YOUR APPLICATIONS</p><h1>One engine. Your software.</h1><p>No predefined workflow catalogue. Each application carries its own data model, pages, and capability requirements.</p></div><button class="primary" onclick={()=>mode='create'}>New application</button></div>
 <div class="app-grid">{#each apps as app}<button class="panel app-card" onclick={()=>void task(()=>open(app.id))}><span class="app-icon">{app.name.slice(0,1)}</span><h2>{app.name}</h2><p>{app.key}</p><small>Version {app.version} · persistent application</small></button>{/each}</div>
 {#if !apps.length}<section class="empty-state"><h2>Start with the application you need.</h2><p>Create collections and their fields, add pages, and declare any abilities—even ones that are not installed yet.</p><button class="secondary" onclick={()=>mode='create'}>Design an application</button></section>{/if}
 {:else if mode==='create'}<section class="panel">{#key mode}<AppDesigner onsave={save} {busy}/>{/key}</section>
 {:else if mode==='edit'&&current}<section class="panel">{#key current.id}<AppDesigner initial={current.spec} onsave={save} {busy}/>{/key}</section>
 {:else if current}<div class="page-heading"><div><p class="eyebrow">APPLICATION V{current.version}</p><h1>{current.spec.name}</h1><p>{current.spec.description??'Your data model, interface, and capability ports.'}</p></div><button class="secondary" onclick={()=>mode='edit'}>Edit application</button></div>
 {#key `${current.id}:${current.version}`}<AppWorkspace app={current} {api} {upload} {download} onerror={message=>error=message}/>{/key}
 {/if}
{/if}
</main>
