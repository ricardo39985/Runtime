export type Field = {key:string;label:string;type:'text'|'multiline'|'integer'|'decimal'|'boolean'|'date'|'select'|'reference'|'file';required?:boolean;options?:string[];entity?:string};
export type Entity = {key:string;label:string;fields:Field[];access:{read:string[];write:string[]}};
export type Action = {key:string;label:string;entity:string;capability:string;version:number;contract:string;resolution?:{available:boolean;reason:string}};
export type AppSpec = {schema_version:1;key:string;name:string;description?:string;roles:string[];entities:Entity[];pages:{key:string;title:string;entity:string;actions:string[]}[];actions:Action[]};
export type AppDocument = {id:string;version:number;spec:AppSpec;ui:{name:string;pages:{key:string;title:string;entity:Entity;actions:Action[];can_write:boolean}[];capabilities:{action:string;capability:string;available:boolean;reason:string}[]}};
export type RecordValue = string|number|boolean|null;
export type AppRecord = {id:string;version:number;data:Record<string,RecordValue>};
export type Asset = {id:string;filename:string;sha256:string;declared_mime:string;codec:string;original_size:number;stored_size:number};
export const fieldTypes:Field['type'][]=['text','multiline','integer','decimal','boolean','date','select','reference','file'];
export function emptyApplication(name:string,key:string):AppSpec {
  return {schema_version:1,key,name,roles:['owner','editor','viewer'],entities:[{key:'items',label:'Items',fields:[{key:'name',label:'Name',type:'text',required:true}],access:{read:['owner','editor','viewer'],write:['owner','editor']}}],pages:[{key:'items',title:'Items',entity:'items',actions:[]}],actions:[]};
}
export function keyFor(text:string) {const value=text.toLowerCase().replace(/[^a-z0-9_]+/g,'_').replace(/^_+|_+$/g,'').slice(0,60);return /^[a-z]/.test(value)?value:`app_${value}`;}
export function bytes(value:number) {return value>=1024*1024?`${(value/1024/1024).toFixed(2)} MiB`:value>=1024?`${(value/1024).toFixed(1)} KiB`:`${value} B`;}
