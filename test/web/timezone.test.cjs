const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');const vm=require('node:vm');const path=require('node:path');
const source=fs.readFileSync(path.join(__dirname,'../../web/index.html'),'utf8');
test('rendering Today from browsers in different time zones never writes config',()=>{
  for(const offset of [-660,0,60,330,840]) {
    const posts=[],elements=new Map(),$=s=>{if(!elements.has(s))elements.set(s,{replaceChildren(){}});return elements.get(s);};
    const zero={heat_kwh:0,elec_kwh:0};
    class Clock extends Date {getTimezoneOffset(){return -offset;}}
    const context={Date:Clock,$,el:()=>({}),t:s=>s,cfg:{stats:{}},status:{stats:{tz:60,mode:'heating',today:{heating:zero,dhw:zero,cooling:zero}}},
      post:(...args)=>{posts.push(args);return Promise.resolve();}};
    vm.runInNewContext(source.match(/^function renderToday\(\) \{[\s\S]*?^\}/m)[0]+'\nrenderToday();renderToday();',context);
    assert.deepEqual(posts,[],`browser UTC offset ${offset}`);
  }
});
test('using the browser offset only edits the form; explicit Save sends the value',async()=>{
  const elements=new Map(),posts=[],$=s=>{if(!elements.has(s))elements.set(s,{value:'',checked:false});return elements.get(s);};
  class Clock extends Date {getTimezoneOffset(){return -330;}}
  const context={Date:Clock,$,post:async(p,d)=>{posts.push({p,d});return {};},loadConfig:async()=>{},cfg:{lang:'en'},lang:'en',t:s=>s,msg:()=>{}};
  const choose=source.match(/^\$\('#sBrowserTz'\)\.onclick = .*$/m);
  assert.ok(choose,'explicit browser-offset control');vm.runInNewContext(choose[0],context);
  $('#sBrowserTz').onclick();assert.equal($('#sStatsTz').value,330);assert.equal(posts.length,0);
  const save=source.slice(source.indexOf("$('#sSave').onclick ="),source.indexOf('// ---- diagnostics'));
  vm.runInNewContext(save,context);await $('#sSave').onclick();
  assert.equal(posts.length,1);assert.equal(posts[0].p,'/api/config');assert.equal(posts[0].d.stats.tz,330);
});
