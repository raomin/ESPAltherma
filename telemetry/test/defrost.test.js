import {test} from 'node:test';
import assert from 'node:assert/strict';
import {onRequestPost, onRequestGet, validateCase} from '../functions/api/defrost.js';
const fixture = () => ({v:1,user_consent:true,install_id:'0123456789abcdef',boot_id:23,model:'ERGA',fw:'test',board:'esp32',
  event:{id:420000,detected_epoch:0,observed_start_uptime_ms:10000,elapsed_s:410,start_uncertainty_s:30,
    outdoor_c:2,coil_c:-7,observation:'ice_free',truncated:false,recording_complete:true},
  identity:{hydro_software:'20002203'},samples:Array.from({length:181}, (_,i) => [i?5:0,4,-21,40,16|(i>=82?2:1)])});
function db(count=0) {
  const writes=[];
  return {writes,prepare(sql) {return {bind(...args) {return {first:async()=>count,run:async()=>{writes.push({sql,args});}};}};}};
}
function send(body=fixture(), env={DB:db()}, headers={'content-type':'application/json'}) {
  return onRequestPost({request:new Request('https://example.test/api/defrost',{method:'POST',headers,body:typeof body==='string'?body:JSON.stringify(body)}),env});
}
test('full 15-minute case and observation are stored; unrelated metadata is stripped', async () => {
  const p=fixture(), DB=db();p.ip='secret';p.identity.password='secret';p.event.network='secret';
  const response=await send(p,{DB});assert.equal(response.status,200);assert.equal(DB.writes.length,1);
  const stored=JSON.parse(DB.writes[0].args[5]);assert.equal(stored.samples.length,181);
  assert.equal(stored.event.observation,'ice_free');assert.ok(!JSON.stringify(stored).includes('secret'));
  assert.equal(stored.temperature_scale_c,0.5);
});
test('no database writes without explicit case consent', async () => {
  for (const consent of [undefined,false,'true',1]) {
    const p=fixture(), DB=db();p.user_consent=consent;
    assert.equal((await send(p,{DB})).status,400);assert.equal(DB.writes.length,0);
  }
});
test('reject invalid times, unbounded traces, observations and non-integer samples', async () => {
  for (const mutate of [p=>p.samples.push([5,0,0,0,0]),p=>p.samples[0][0]=5,p=>p.samples[1][1]=-129,
    p=>p.samples[1][3]=NaN,p=>p.samples[1][4]=64,p=>p.event.elapsed_s=901,p=>p.event.observation='assumed_ice_free',
    p=>p.event.start_uncertainty_s=600,p=>p.install_id='invalid',p=>p.samples[1][0]=254]) {
    const p=fixture();mutate(p);assert.equal((await send(p)).status,400);
  }
});
test('unknown readings and incomplete traces remain honest', () => {
  const p=fixture();p.samples=[[0,-128,-128,255,0]];p.event.outdoor_c=null;p.event.coil_c=null;
  p.event.observation='not_checked';p.event.truncated=true;p.event.recording_complete=false;
  assert.deepEqual(validateCase(p).samples,p.samples);
});
test('method, content type, malformed JSON and body limit', async () => {
  assert.equal(onRequestGet().status,405);
  assert.equal((await send(fixture(),{}, {'content-type':'text/plain'})).status,415);
  assert.equal((await send('{')).status,400);
  assert.equal((await send(' '.repeat(8193))).status,413);
  assert.equal((await send(fixture(),{}, {'content-type':'application/json','content-length':'9000'})).status,413);
});
test('rate limits and unavailable storage never accept a report', async () => {
  const DB=db(500);assert.equal((await send(fixture(),{DB})).status,429);assert.equal(DB.writes.length,0);
  assert.equal((await send(fixture(),{})).status,503);
});
