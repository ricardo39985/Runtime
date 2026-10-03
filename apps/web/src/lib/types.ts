export type Invoice = { id: string; customer: string; email: string; amount_minor: string; currency: string; due_date: string };
export type UIComponent = { type: 'metric'; label: string; binding: string } | { type: 'table'; binding: 'invoices' };
export type UISpec = { schema_version: 1; title: string; components: UIComponent[] };
export type Draft = { invoice_id: string; to: string; subject: string; body: string };
export type Run = {
  id: string; status: string; version: number; proposal_hash: string; expires_at: number;
  title?: string; ui?: UISpec; bindings?: Record<string, unknown>; drafts?: Draft[];
  excluded?: number; not_in_batch?: number; source?: { kind: string; captured_at: number; as_of: string };
  receipts?: { invoice_id: string; status: string; external_action: false }[]; error?: string;
};
export type RunSummary = { id: string; title: string; status: string };
export type RunEvent = { sequence: string; kind: string; at: string };
export function money(minor: string, currency: string): string {
  const precisions: Record<string, number> = { USD: 2, EUR: 2, GBP: 2, GYD: 2, CAD: 2, JPY: 0, KWD: 3 };
  const digits = precisions[currency];
  if (digits === undefined || !/^\d+$/.test(minor)) return `${minor} ${currency} minor units`;
  const value = BigInt(minor), divisor = 10n ** BigInt(digits);
  const whole = (value / divisor).toLocaleString('en-US');
  const fraction = digits ? `.${(value % divisor).toString().padStart(digits, '0')}` : '';
  return `${currency} ${whole}${fraction}`;
}
