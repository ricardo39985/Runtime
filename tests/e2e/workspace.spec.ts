import { test, expect } from '@playwright/test';
import type { Page } from '@playwright/test';
function blueprint() {
 const key='app_'+crypto.randomUUID().replaceAll('-','');
 return {schema_version:1,key,name:'Independent creative workspace',roles:['owner','viewer'],entities:[{key:'projects',label:'Projects',fields:[{key:'title',label:'Title',type:'text',required:true},{key:'asset',label:'Asset',type:'file'}],access:{read:['owner','viewer'],write:['owner']}}],pages:[{key:'projects',title:'Projects',entity:'projects',actions:['paint']}],actions:[{key:'paint',label:'Paint project',entity:'projects',capability:'media.paint',version:1,contract:'record-in/result-out@1'}]};
}
async function create(page:Page) {
 await page.goto('/');
 await page.getByLabel('Development access token').fill(process.env.DEV_API_TOKEN!);
 await page.getByRole('button',{name:'Open workspace',exact:true}).click();
 await page.getByRole('button',{name:'New application',exact:true}).click();
 await page.getByRole('button',{name:'Application JSON',exact:true}).click();
 await page.getByLabel('Application specification').fill(JSON.stringify(blueprint()));
 await page.getByRole('button',{name:'Create application',exact:true}).click();
 await expect(page.getByRole('heading',{name:'Independent creative workspace',exact:true})).toBeVisible();
 await page.locator('.record-form').getByLabel('Title',{exact:false}).fill('My persistent project');
 await page.getByRole('button',{name:'Create record',exact:true}).click();
 await expect(page.getByRole('cell',{name:'My persistent project',exact:true})).toBeVisible();
}
test('generic app, missing ability, persistent record and lossless file',async({page})=>{
 const errors:string[]=[]; page.on('pageerror',e=>errors.push(e.message));
 await create(page);
 await expect(page.getByText('Not installed',{exact:true})).toBeVisible();
 await page.getByRole('button',{name:'Files & storage',exact:true}).click();
 await page.locator('input[type=file]').setInputFiles({name:'design-source.txt',mimeType:'text/plain',buffer:Buffer.from('Original application asset\n'.repeat(2048))});
 await expect(page.getByRole('cell',{name:/^design-source\.txt/})).toBeVisible();
 await expect(page.getByRole('cell',{name:'zstd',exact:true})).toBeVisible();
 const waiting=page.waitForEvent('download');
 await page.getByRole('button',{name:'Download',exact:true}).click();
 const download=await waiting; expect(download.suggestedFilename()).toBe('design-source.txt');
 await page.screenshot({path:'evidence/screenshots/storage-desktop.png',fullPage:true});
 await page.getByRole('button',{name:'Projects',exact:true}).click();
 await page.screenshot({path:'evidence/screenshots/application-desktop.png',fullPage:true});
 await page.getByRole('button',{name:'Applications',exact:true}).click();
 await page.getByRole('button',{name:/Independent creative workspace/}).last().click();
 await expect(page.getByRole('cell',{name:'My persistent project',exact:true})).toBeVisible();
 expect(errors).toEqual([]);
});
test('mobile app uses same generic renderer without page overflow',async({page})=>{
 await page.setViewportSize({width:390,height:844}); await create(page);
 expect(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth)).toBe(false);
 await page.screenshot({path:'evidence/screenshots/application-mobile.png',fullPage:true});
});
