<script lang="ts">
  import validate from './ui-validator.generated.js';
  import { money, type Invoice, type UISpec } from './types';
  let { spec, bindings }: { spec: UISpec; bindings: Record<string, unknown> } = $props();
  let valid = $derived(validate(spec));
  function amount(key: string) {
    const value = bindings[key] as {amount_minor?: string; currency?: string} | undefined;
    return value?.amount_minor && value.currency ? money(value.amount_minor, value.currency) : 'Unavailable';
  }
  function rows(key: string) { return Array.isArray(bindings[key]) ? bindings[key] as Invoice[] : []; }
</script>
{#if !valid}
  <div class="error" role="alert">This workspace has an unsupported interface description. Nothing was executed.</div>
{:else}
  <div class="metrics">
    {#each spec.components.filter(c => c.type === 'metric') as item}
      {#if item.type === 'metric'}<article class="metric"><span>{item.label}</span><strong>{amount(item.binding)}</strong><small>From your imported snapshot</small></article>{/if}
    {/each}
  </div>
  {#each spec.components as item}
    {#if item.type === 'table'}
      <article class="panel invoice-panel"><div class="panel-heading"><h3>Invoices needing attention</h3><span class="count">{rows(item.binding).length}</span></div>
      <div class="table-scroll"><table><thead><tr><th>Customer</th><th>Invoice</th><th>Due date</th><th class="number">Outstanding</th></tr></thead><tbody>
        {#each rows(item.binding) as invoice (invoice.id)}<tr><td><strong>{invoice.customer}</strong><small>{invoice.email}</small></td><td>{invoice.id}</td><td>{invoice.due_date}</td><td class="number">{money(invoice.amount_minor,invoice.currency)}</td></tr>{/each}
      </tbody></table></div>
      {#if rows(item.binding).length === 0}<p class="empty">No invoices match the selected overdue threshold.</p>{/if}
      </article>
    {/if}
  {/each}
{/if}
