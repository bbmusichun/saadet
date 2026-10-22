// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// Real firmware capability/archive fixtures and an independent wire peer.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import vm from 'node:vm';
import {BACKUP_IDS,bkManifest,bkU32,bkR32,bkPack,bkUnpack,bkCrc,readBackup,captureBackup,restoreBackup} from './fm1backup.js';
const html=readFileSync(new URL('editor.html',import.meta.url),'utf8');
const proto=html.split('/*PROTO-BEGIN*/')[1].split('/*PROTO-END*/')[0];
const E=vm.runInNewContext(`${proto};({parse,CMD,engineOrder,FM6,CZ,P5,bank,cleanPatch,libraryFile})`,{setTimeout,clearTimeout,setInterval,clearInterval,console});
const info=E.parse[E.CMD.INFO](Array.from(readFileSync('build/host/synth-info.bin')));
assert(info.synthOnly && info.ntrk===1 && info.nstep===0);
assert(info.fm6 && info.cz && info.prophet && info.backupCaps===3);
assert.equal(info.uiCaps&16,0);assert.equal(info.shared.total,384);assert.equal(info.native,null);
assert.equal(E.engineOrder(info.engines).map(i=>info.engines[i]).join(),'PROPHET,FM6,CZ-1,SID');
assert(!info.motionMax && !info.patterns && !info.chainRows);
const tabs=[...html.matchAll(/<button[^>]+data-tab="([^"]+)"/g)].map(m=>m[1]);
assert.deepEqual(tabs,['sound','fm6','library','settings']);
for(const id of ['p-sequencer','p-tracks','p-projects','trackbar','steptables'])assert(!html.includes(`id="${id}"`));
const ids=new Set([...html.matchAll(/\bid="([^"]+)"/g)].map(m=>m[1]));
for(const m of html.matchAll(/\$\("([^"]+)"\)/g))assert(ids.has(m[1]),`Missing DOM element: ${m[1]}`);
assert(html.indexOf('id="backup-panel"')>html.indexOf('id="p-settings"'));
const sent=[];const libraryPeer=async([cmd,a])=>{sent.push([cmd,Array.from(a)]);return [a[0],a[1],a[2],0];};
await E.bank.store(libraryPeer,383,'LAST',info,0);await E.bank.load(libraryPeer,383,info,0);
assert.equal(sent[0][0],80);assert.deepEqual(sent[0][1].slice(0,4),[3,127,2,0]);assert.deepEqual(sent[1],[80,[4,127,2,0]]);
assert.throws(()=>E.cleanPatch({engine:10,p:[]}));
const clean=E.cleanPatch({engine:3,p:[],pattern:[[60,1]],grid:[[1,1]]});assert(!clean.pattern && !clean.grid);
const exported=E.libraryFile('bank',[clean],{}).patches[0];assert(!('pattern' in exported) && !('grid' in exported));
const file={format:'saadet-backup',version:1,objects:BACKUP_IDS.map(id=>{
 const bytes=readFileSync(`build/host/archive-${id}.bin`);return {id,size:bytes.length,crc:bkCrc(bytes),data:bytes.toString('base64')};
})};
readBackup(file);
const manifest=[1,0,BACKUP_IDS.length,...file.objects.flatMap(o=>[o.id,...bkU32(o.size),...bkU32(o.crc)])];
assert.equal(bkManifest(manifest).length,24);
const committed=[],writes=new Map(),staged=new Map();
async function peer([cmd,a]){
 if(cmd===65)return manifest;
 if(cmd===66){const id=a[0],off=bkR32(a,1),n=a[6]|a[7]<<7;const bytes=Buffer.from(file.objects.find(o=>o.id===id).data,'base64');return [id,0,...bkU32(off),n&127,n>>7,...bkPack(bytes.subarray(off,off+n))];}
 assert.equal(cmd,67);const [op,id]=a;assert(BACKUP_IDS.includes(id));
 if(op===0)staged.set(id,{size:bkR32(a,2),crc:bkR32(a,7),bytes:[]});
 if(op===1){const st=staged.get(id);assert.equal(bkR32(a,2),st.bytes.length);const n=Math.min(256,st.size-st.bytes.length);st.bytes.push(...bkUnpack(a.slice(7),n));}
 if(op===2){const st=staged.get(id);assert.equal(st.bytes.length,st.size);assert.equal(bkCrc(st.bytes),st.crc);committed.push(id);writes.set(id,Buffer.from(st.bytes));staged.delete(id);}
 if(op===3)staged.delete(id);
 return [op,id,0];
}
const captured=await captureBackup(peer,'test');assert.deepEqual(captured.objects,file.objects);
await restoreBackup(peer,file);assert.deepEqual(committed.slice(-2),[1,32]);assert.equal(committed.length,24);
for(const o of file.objects)assert.deepEqual(writes.get(o.id),Buffer.from(o.data,'base64'));
const reject=structuredClone(file);reject.objects[0].id=2;assert.throws(()=>readBackup(reject));
assert.throws(()=>readBackup({...file,format:'felucca-backup'}));
for(const id of [1,28,32]){
 const bad=structuredClone(file),o=bad.objects.find(o=>o.id===id);let b=Buffer.from(o.data,'base64');if(!b.length)b=Buffer.alloc(3716);b[0]^=1;o.size=b.length;o.crc=bkCrc(b);o.data=b.toString('base64');assert.throws(()=>readBackup(bad));
}
let commits=0;await assert.rejects(restoreBackup(async([cmd,a])=>cmd===65?manifest:(a[0]===2&&(commits++),[a[0],a[1],a[0]===0&&a[1]===32?2:0]),file));assert.equal(commits,0);
for(const n of [0,1,7,8,256]){const bytes=Uint8Array.from({length:n},(_,i)=>i*137);assert.deepEqual(bkUnpack(bkPack(bytes),n),bytes);}
assert.throws(()=>bkUnpack([128,0],1));assert.throws(()=>bkManifest(manifest.slice(0,-1)));
console.log('SAADET WEB: real INFO, four tabs, DOM references, 384 slots, archive round trip, preflight, corrupt/foreign rejection passed');
