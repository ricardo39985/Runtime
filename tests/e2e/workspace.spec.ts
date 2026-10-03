import { test, expect } from '@playwright/test';
import { readFileSync, mkdirSync } from 'node:fs';

const creative = JSON.parse(readFileSync('examples/creative-studio.json', 'utf8'));
const editorial = JSON.parse(readFileSync('examples/editorial-desk.json', 'utf8'));

async function signIn(page: import('@playwright/test').Page) {
  await page.goto('/');
  await page.getByLabel('Development access token').fill(process.env.DEV_API_TOKEN!);
  await page.getByRole('button', { name: 'Open studio' }).click();
  await expect(page.getByRole('heading', { name: 'Applications', exact: true })).toBeVisible();
}
async function publish(page: import('@playwright/test').Page, spec: unknown) {
  await page.getByRole('button', { name: 'Build an application' }).click();
  await page.getByLabel('Application JSON').fill(JSON.stringify(spec));
  await page.getByRole('button', { name: 'Validate definition' }).click();
  await expect(page.getByText('Valid structure', { exact: true })).toBeVisible();
  await page.getByRole('button', { name: 'Publish application' }).click();
}

test('general application, missing ability, real records, files and reopen', async ({ page }) => {
  const errors: string[] = [];
  page.on('pageerror', error => errors.push(error.message));
  await signIn(page);
  await publish(page, { ...creative, name: 'Browser creative workspace' });
  await expect(page.getByRole('heading', { name: 'Project library', exact: true })).toBeVisible();
  await page.getByLabel('Project name').fill('Persistent browser project');
  await page.getByLabel('Creative brief').fill('A project whose missing paint ability does not prevent saving.');
  await page.getByRole('button', { name: 'Save record', exact: false }).click();
  await expect(page.locator('table').getByText('Persistent browser project', { exact: true })).toBeVisible();
  await page.getByRole('button', { name: 'Paint', exact: true }).click();
  await expect(page.getByRole('button', { name: 'Paint artwork' })).toBeDisabled();
  await expect(page.getByText('Requires media.paint@1.', { exact: false })).toBeVisible();
  await page.getByRole('button', { name: 'Assets', exact: true }).click();
  const payload = Buffer.from('Lossless persistent storage.\n'.repeat(3000));
  await page.getByLabel('Upload file', { exact: true }).setInputFiles({ name: 'creative-notes.txt', mimeType: 'text/plain', buffer: payload });
  await expect(page.getByText('creative-notes.txt', { exact: true })).toBeVisible();
  const downloadPromise = page.waitForEvent('download');
  await page.getByRole('button', { name: 'Download', exact: true }).click();
  const download = await downloadPromise;
  expect(readFileSync((await download.path())!)).toEqual(payload);
  await page.getByRole('button', { name: 'Projects', exact: true }).click();
  mkdirSync('evidence/screenshots', { recursive: true });
  await page.screenshot({ path: 'evidence/screenshots/application-desktop.png', fullPage: true });
  await page.reload();
  await page.getByLabel('Development access token').fill(process.env.DEV_API_TOKEN!);
  await page.getByRole('button', { name: 'Open studio' }).click();
  await page.getByRole('button', { name: /Browser creative workspace/ }).click();
  await expect(page.locator('table').getByText('Persistent browser project', { exact: true })).toBeVisible();
  expect(errors).toEqual([]);
});

test('a different application uses the same renderer and installed capability', async ({ page }) => {
  const errors: string[] = [];
  page.on('pageerror', error => errors.push(error.message));
  await signIn(page);
  await publish(page, { ...editorial, name: 'Browser editorial workspace' });
  await expect(page.getByRole('heading', { name: 'Editorial desk', exact: true })).toBeVisible();
  await page.getByLabel('Title', { exact: false }).fill('Native generic capability');
  await page.getByLabel('Article copy').fill('one two three four');
  await page.getByRole('button', { name: 'Save record', exact: false }).click();
  await expect(page.locator('table').getByText('Native generic capability')).toBeVisible();
  await page.getByLabel('Target record').selectOption({ index: 1 });
  await page.getByRole('button', { name: 'Count words' }).click();
  await expect(page.locator('table').getByRole('cell', { name: '4', exact: true })).toBeVisible();
  await page.setViewportSize({ width: 390, height: 844 });
  expect(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth)).toBe(true);
  await page.screenshot({ path: 'evidence/screenshots/application-mobile.png', fullPage: true });
  expect(errors).toEqual([]);
});
