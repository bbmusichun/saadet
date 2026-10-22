// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
import {JSDOM} from 'jsdom';
import {readFileSync} from 'node:fs';
const html=readFileSync('web/editor.html','utf8');
const dom=new JSDOM(html,{url:'http://localhost/web/editor.html',runScripts:'outside-only'});
dom.window.eval(html.split('<script>')[1].split('</script>')[0]);
const document=dom.window.document;
for(const name of ['sound','fm6','library','settings']){
 const button=document.querySelector(`button[data-tab="${name}"]`);button.click();
 if(button.getAttribute('aria-selected')!=='true')throw Error('Tab did not select: '+name);
}
console.log('Editor DOM startup, localization and four tab actions passed');dom.window.close();
