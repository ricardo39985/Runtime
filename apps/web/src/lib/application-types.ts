export type Scalar=string|boolean|null;
export type Field={id:string;label:string;type:'text'|'integer'|'boolean'|'date'|'choice'|'reference'|'file';required?:boolean;unique?:boolean;max_length?:number;options?:string[];target?:string;default?:Scalar};
export type Entity={id:string;label:string;fields:Field[];read_roles:string[];write_roles:string[]};
export type Block={kind:'table'|'form'|'cards'|'files'|'action';title?:string;entity?:string;action?:string;fields?:string[]};
export type Capability={id:string;description:string;inputs:Field[];outputs:Field[]};
export type Action={id:string;label:string;entity:string;capability:string;inputs:Record<string,string>;outputs:Record<string,string>;roles:string[]};
export type AppSpec={schema_version:1;name:string;description?:string;roles:string[];entities:Entity[];pages:{id:string;title:string;blocks:Block[]}[];capabilities:Capability[];actions:Action[];storage?:{read_roles:string[];write_roles:string[];max_file_bytes?:number;max_space_bytes?:number}};
export type AppRecord={id:string;version:number;values:Record<string,Scalar>};
export type Asset={id:string;filename:string;original_size:string;stored_size:string;codec:string;sha256:string;state:string};
export type AppDocument={id:string;revision:number;spec:AppSpec;capabilities:{id:string;description:string;status:'available'|'missing'|'incompatible'}[];spaces:{id:string;name:string;role:string}[];can_edit_schema:boolean};
export type PageOfRecords={items:AppRecord[];next_cursor:string|null};
export function defaults(fields:Field[]):Record<string,Scalar>{return Object.fromEntries(fields.map(f=>[f.id,f.default??(f.type==='boolean'?false:null)]));}
export function visible(value:Scalar|undefined){return value===null||value===undefined?'—':typeof value==='boolean'?(value?'Yes':'No'):value;}
export function size(value:string|number){const n=Number(value);return n<1024?`${n} B`:n<1048576?`${(n/1024).toFixed(1)} KiB`:`${(n/1048576).toFixed(2)} MiB`;}
