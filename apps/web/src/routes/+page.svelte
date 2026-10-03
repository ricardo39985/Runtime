<script lang="ts">
  import { onMount } from 'svelte';
  import Workspace from '$lib/Workspace.svelte';
  import type { Run, RunSummary, RunEvent } from '$lib/types';
  import '../style.css';
  let token = $state(''), authenticated = $state(false), busy = $state(false), error = $state('');
  let tab = $state('workspace'), title = $state('Overdue invoice follow-up'), days = $state(14), csv = $state('');
  let run = $state<Run | null>(null), history = $state<RunSummary[]>([]), events = $state<RunEvent[]>([]);
  let acknowledged = $state(false), edits = $state<Record<number, string>>({});
  let pendingKey: { fingerprint: string; key: string } | null = null;
  let dirty = $derived(Object.keys(edits).length > 0);
  let active = $derived(!!run && ['queued', 'preparing', 'executing'].includes(run.status));
  async function api<T>(path: string, method = 'GET', body?: unknown, key?: string): Promise<T> {
    const headers: Record<string, string> = { authorization: `Bearer ${token}` };
    if (body !== undefined) headers['content-type'] = 'application/json';
    if (key) headers['idempotency-key'] = key;
    const response = await fetch(`/api/v1/${path}`, { method, headers, body: body === undefined ? undefined : JSON.stringify(body), signal: AbortSignal.timeout(10000) });
    const data = await response.json();
    if (!response.ok) throw new Error(data.error?.message ?? `Request failed (${response.status}).`);
    return data as T;
  }
  async function attempt(operation: () => Promise<void>) {
    if (busy) return; busy = true; error = '';
    try { await operation(); } catch (failure) { error = failure instanceof Error ? failure.message : 'Request failed.'; } finally { busy = false; }
  }
  async function refreshHistory() { history = await api<RunSummary[]>('runs'); }
  async function openRun(id: string) {
    run = await api<Run>(`runs/${id}`); events = await api<RunEvent[]>(`runs/${id}/events`);
    edits = {}; acknowledged = false; tab = 'workspace';
  }
  function login() { void attempt(async () => { await api('meta'); authenticated = true; const sample = await api<{csv: string}>('sample'); csv = sample.csv; await refreshHistory(); }); }
  function start() { void attempt(async () => {
    const body = {schema_version: 1, title, min_days: days, csv}; const fingerprint = JSON.stringify(body);
    if (pendingKey?.fingerprint !== fingerprint) pendingKey = {fingerprint, key: crypto.randomUUID()};
    run = await api<Run>('requests', 'POST', body, pendingKey!.key); pendingKey = null;
    edits = {}; events = []; acknowledged = false; await refreshHistory();
  }); }
  function saveDraft(index: number) { void attempt(async () => {
    if (!run) return;
    run = await api<Run>(`runs/${run.id}/drafts`, 'PATCH', {version: run.version, index, body: edits[index]});
    const next = {...edits}; delete next[index]; edits = next; acknowledged = false;
  }); }
  function approve() { void attempt(async () => {
    if (!run || dirty || !acknowledged) return;
    run = await api<Run>(`runs/${run.id}/approve`, 'POST', {version: run.version, proposal_hash: run.proposal_hash, snapshot_ack: acknowledged});
    await refreshHistory();
  }); }
  async function importFile(event: Event) {
    const file = (event.currentTarget as HTMLInputElement).files?.[0]; if (!file) return;
    await attempt(async () => { if (file.size > 256 * 1024) throw new Error('CSV limit is 256 KiB.'); csv = await file.text(); });
  }
  onMount(() => {
    let polling = false;
    const timer = setInterval(async () => {
      if (!authenticated || !run || !active || polling || busy) return;
      const id = run.id; polling = true;
      try {
        const next = await api<Run>(`runs/${id}`); const nextEvents = await api<RunEvent[]>(`runs/${id}/events`);
        if (run?.id === id) { run = next; events = nextEvents; }
        await refreshHistory();
      } catch (failure) { error = failure instanceof Error ? failure.message : 'Reconnect failed.'; }
      finally { polling = false; }
    }, 1000);
    return () => clearInterval(timer);
  });
</script>
<svelte:head><title>Runtime — Your operational workspace</title><meta name="description" content="Runtime development workspace: validated workflows, generated interfaces, and explicit approvals."/></svelte:head>
<div class="app-shell">
  <aside class="sidebar">
    <a href="/" class="brand" aria-label="Runtime home"><span class="brand-mark">r<span>_</span></span>Runtime<span class="version">0.1</span></a>
    <div class="workspace-switch"><span class="avatar">D</span><div>Development<small>Personal workspace</small></div><span class="chevron">⌄</span></div>
    <div class="nav-label">WORKSPACE</div>
    <nav aria-label="Main navigation">
      <button class:chosen={tab === 'workspace'} onclick={() => tab = 'workspace'}><span>▦</span>Overview</button>
      <button class:chosen={tab === 'history'} onclick={() => {tab = 'history'; if (authenticated) void attempt(refreshHistory);}}><span>↻</span>Run history</button>
      <button class:chosen={tab === 'connections'} onclick={() => tab = 'connections'}><span>⌘</span>Connections</button>
    </nav>
    <div class="sidebar-bottom"><span class="status-dot"></span>Local development<small>Fixture providers · no external actions</small></div>
  </aside>
  <main>
    <header class="topbar"><div>Workspace <span>/</span> {tab === 'history' ? 'Run history' : tab === 'connections' ? 'Connections' : 'Overview'}</div><span class="environment">DEVELOPMENT</span></header>
    <div class="content">
      {#if error}<div class="error" role="alert">{error}<button aria-label="Dismiss error" onclick={() => error = ''}>×</button></div>{/if}
      {#if !authenticated}
        <section class="welcome"><div class="eyebrow">YOUR SOFTWARE. ASSEMBLED AROUND YOU.</div><h1>A workspace for<br/>what needs doing.</h1><p>Start with the first working capability: turn an invoice snapshot into a reviewable follow-up workflow.</p>
          <form class="panel login" onsubmit={(e) => {e.preventDefault(); login();}}><h2>Open local workspace</h2><p>Use the development token in your local <code>.env</code> file. Google sign-in is not implemented in this milestone.</p><label for="token">Development access token</label><input id="token" type="password" bind:value={token} autocomplete="off" required minlength="32" placeholder="Paste your local token"/><button class="primary" disabled={busy}>{busy ? 'Opening…' : 'Open workspace'}<span>→</span></button></form>
          <div class="trust-note">No model calls. No emails sent. No paid infrastructure.</div>
        </section>
      {:else if tab === 'connections'}
        <div class="section-title"><div class="eyebrow">CAPABILITIES</div><h1>Connections</h1><p>Only the local CSV workflow is enabled. Provider access is never simulated as a live connection.</p></div>
        <div class="connection-grid">{#each [['CSV snapshots','Enabled locally','Validated imports and deterministic calculations.'],['Jev routing','Adapter written · not connected','Conservative routing remains active until credentials and evaluations are configured.'],['Google Workspace','Not connected','OIDC, Gmail, Calendar, and Sheets are later implementation gates.'],['Stripe','Not connected','Read-only invoice access is planned; no payments or refunds.']] as item}<article class="panel connection"><span class="connection-icon">{item[0].slice(0,1)}</span><h3>{item[0]}</h3><span class="badge">{item[1]}</span><p>{item[2]}</p></article>{/each}</div>
      {:else if tab === 'history'}
        <div class="section-title"><div class="eyebrow">EXECUTION RECORD</div><h1>Run history</h1><p>Persisted in PostgreSQL. Reopening a run does not execute it again.</p></div>
        <section class="panel history">{#each history as item}<button onclick={() => void attempt(() => openRun(item.id))}><div><strong>{item.title}</strong><small>{item.id.slice(0,12)}</small></div><span class="badge">{item.status.replaceAll('_',' ')}</span><span>→</span></button>{/each}{#if history.length === 0}<p class="empty">Your first run will appear here.</p>{/if}</section>
      {:else}
        <div class="section-title"><div><div class="eyebrow">FROM REQUEST TO REVIEWED ACTION</div><h1>Your work, in one place.</h1><p>A deterministic first workflow. Natural-language planning is not connected yet.</p></div><span class="badge subtle">C++ runtime</span></div>
        <section class="panel composer"><div class="composer-title"><span class="workflow-icon">↗</span><div><h2>Invoice follow-up</h2><p>Import a snapshot. Review a generated workspace. Approve a local simulation.</p></div></div>
          <div class="form-row"><div class="field grow"><label for="title">Workflow title</label><input id="title" bind:value={title} maxlength="160"/></div><div class="field"><label for="days">More than this many days overdue</label><input id="days" type="number" bind:value={days} min="0" max="3650"/></div><button class="primary" onclick={start} disabled={busy || active || !csv || !title}>{busy ? 'Preparing…' : 'Build workspace'}<span>→</span></button></div>
          <details><summary>Source data <span>CSV snapshot · inspect or replace</span></summary><label for="csv">Invoice CSV</label><textarea id="csv" bind:value={csv} rows="5" spellcheck="false"></textarea><input type="file" aria-label="Import invoice CSV" accept=".csv,text/csv" onchange={importFile}/><small>Amounts use integer minor units. No automatic currency conversion.</small></details>
        </section>
        {#if run}
          <div class="run-heading"><div><span class="status-dot" class:running={active}></span><strong>{run.title ?? title}</strong><span class="badge">{run.status.replaceAll('_',' ')}</span></div>{#if ['queued','awaiting_approval'].includes(run.status)}<button class="quiet" disabled={busy} onclick={() => void attempt(async () => {if(run) run = await api<Run>(`runs/${run.id}/cancel`, 'POST', {});})}>Cancel run</button>{/if}</div>
          {#if active}<div class="working" role="status"><span class="loader"></span>{run.status === 'executing' ? 'Recording the local simulation…' : 'Validating source data and assembling your workspace…'}</div>{/if}
          {#if run.error}<div class="error" role="alert">{run.error}</div>{/if}
          {#if run.ui && run.bindings}
            <div class="results-grid"><div class="workspace-result"><Workspace spec={run.ui} bindings={run.bindings}/><div class="source-note">CSV snapshot · evaluated on {run.source?.as_of} · {run.excluded ?? 0} records excluded. This is not a live accounting feed.</div>
              <section class="panel timeline"><h3>Run activity</h3>{#each events as item}<div class="event"><span class="event-dot"></span><span>{item.kind.replaceAll('_',' ')}</span><small>{new Date(item.at).toLocaleTimeString([], {hour: '2-digit', minute: '2-digit'})}</small></div>{/each}</section>
            </div><aside class="panel approval"><div class="panel-heading"><h3>{run.status === 'completed' ? 'Workflow complete' : 'Review before action'}</h3><span class="count">{run.drafts?.length ?? 0}</span></div>
              <p class="approval-intro">Simulation only. Approval creates local receipts; it cannot send email.</p>
              {#each run.drafts ?? [] as draft, index}<div class="draft"><div class="draft-label">MESSAGE {index + 1} <span>{draft.invoice_id}</span></div><strong>{draft.to}</strong><p class="subject">{draft.subject}</p><label class="sr-only" for={`draft-${index}`}>Message body {index + 1}</label><textarea id={`draft-${index}`} value={edits[index] ?? draft.body} disabled={run.status !== 'awaiting_approval' || busy} oninput={(event) => {edits = {...edits, [index]: event.currentTarget.value}; acknowledged = false;}} rows="6" maxlength="10000"></textarea>{#if edits[index] !== undefined}<button class="secondary" disabled={busy} onclick={() => saveDraft(index)}>Save draft changes</button>{/if}</div>{/each}
              {#if run.status === 'awaiting_approval'}<div class="approval-footer"><label class="acknowledgement"><input type="checkbox" bind:checked={acknowledged} disabled={dirty}/><span>I reviewed these messages and understand that the source is a CSV snapshot.</span></label><button class="primary full" disabled={!acknowledged || dirty || busy} onclick={approve}>Approve simulation <span>→</span></button><small>Exact content · version {run.version} · approval expires after 30 minutes</small></div>
              {:else if run.status === 'completed'}<div class="completion" role="status"><strong>{run.receipts?.length ?? 0} simulated receipts</strong><span>0 emails sent. No external systems changed.</span></div>{/if}
              {#if (run.not_in_batch ?? 0) > 0}<p class="source-note">{run.not_in_batch} additional invoices are outside this ten-action batch.</p>{/if}
            </aside></div>
          {/if}
        {:else}
          <div class="empty-state"><div class="empty-icon">▦</div><h2>Your next workspace starts here.</h2><p>The sample contains two overdue invoices and one paid invoice.<br/>Build the workspace to see validation, review, and approval working together.</p><div class="steps"><span>01 &nbsp; Read</span><i>→</i><span>02 &nbsp; Assemble</span><i>→</i><span>03 &nbsp; Review</span><i>→</i><span>04 &nbsp; Simulate</span></div></div>
        {/if}
      {/if}
    </div>
  </main>
</div>
