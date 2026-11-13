// Explicit owner-submitted startup-defrost cases. Independent of automatic heartbeats/model reports.
const MAX_BODY = 8192;
const observations = new Set(['not_checked', 'ice_free', 'iced']);
const integer = (n, low, high) => Number.isInteger(n) && n >= low && n <= high;
const short = (s, max) => typeof s === 'string' && s.length <= max && /^[\x20-\x7e]*$/.test(s);
const reply = (status, error) => new Response(JSON.stringify(error ? {ok:false, error} : {ok:true}),
  {status, headers:{'content-type':'application/json'}});

// Only these fields reach D1, even if a client includes additional metadata.
export function validateCase(p) {
  if (!p || p.v !== 1 || p.user_consent !== true) throw Error('explicit consent and v=1 required');
  if (!/^[0-9a-f]{16}$/.test(p.install_id) || typeof p.install_id !== 'string') throw Error('install_id');
  if (!short(p.model, 80) || !p.model || !short(p.fw, 32) || !short(p.board, 32)) throw Error('model/fw/board');
  if (!integer(p.boot_id, 0, 0xffffffff)) throw Error('boot_id');
  const e = p.event;
  if (!e || !integer(e.id, 0, 0xffffffff) || !integer(e.detected_epoch, 0, 0xffffffff) ||
      !integer(e.observed_start_uptime_ms, 0, 0xffffffff) || !integer(e.elapsed_s, 0, 900) ||
      !integer(e.start_uncertainty_s, 0, 900) || e.elapsed_s + e.start_uncertainty_s > 900 ||
      !observations.has(e.observation) || typeof e.truncated !== 'boolean' || typeof e.recording_complete !== 'boolean') throw Error('event');
  for (const key of ['outdoor_c', 'coil_c'])
    if (e[key] !== null && (typeof e[key] !== 'number' || !Number.isFinite(e[key]) || Math.abs(e[key]) > 64)) throw Error(key);
  if (!Array.isArray(p.samples) || !p.samples.length || p.samples.length > 181) throw Error('samples');
  let seconds = 0;
  for (const [i, row] of p.samples.entries()) {
    if (!Array.isArray(row) || row.length !== 5 || !integer(row[0], i ? 1 : 0, i ? 254 : 0) ||
        !integer(row[1], -128, 127) || !integer(row[2], -128, 127) || !integer(row[3], 0, 255) || !integer(row[4], 0, 63)) throw Error('sample');
    seconds += row[0];
    if (seconds > 900) throw Error('sample time');
  }
  const identity = {};
  for (const key of ['hydro_software', 'hydro_software_source', 'outdoor_eeprom', 'outdoor_mpu_id', 'control_profile', 'reported_indoor_model']) {
    const value = p.identity?.[key] ?? null;
    if (value !== null && !short(value, 80)) throw Error('identity');
    identity[key] = value;
  }
  const revision = p.identity?.outdoor_eeprom_revision_raw ?? null;
  if (revision !== null && !integer(revision, 0, 255)) throw Error('identity revision');
  identity.outdoor_eeprom_revision_raw = revision;
  return {v:1, install_id:p.install_id, boot_id:p.boot_id, model:p.model, fw:p.fw, board:p.board,
    event:Object.fromEntries(['id', 'detected_epoch', 'observed_start_uptime_ms', 'elapsed_s', 'start_uncertainty_s',
      'outdoor_c', 'coil_c', 'observation', 'truncated', 'recording_complete'].map(k => [k, e[k]])), identity,
    sample_columns:'delta_s,outdoor_half_c,coil_half_c,rps,flags', temperature_scale_c:0.5,
    temperature_unknown:-128, rps_unknown:255, flags:{startup:1,defrost:2,dhw:4,cooling:8,status_known:16,dhw_known:32}, samples:p.samples};
}

export const onRequestGet = () => new Response('POST only', {status:405, headers:{allow:'POST'}});
export async function onRequestPost({request, env}) {
  if (!(request.headers.get('content-type') || '').toLowerCase().startsWith('application/json')) return reply(415, 'content type');
  if (Number(request.headers.get('content-length')) > MAX_BODY) return reply(413, 'too large');
  const reader = request.body?.getReader();
  let bytes = 0, text = '';
  if (!reader) return reply(400, 'empty body');
  const decoder = new TextDecoder();
  for (;;) {
    const {done, value} = await reader.read();
    if (done) break;
    bytes += value.length;
    if (bytes > MAX_BODY) {await reader.cancel(); return reply(413, 'too large');}
    text += decoder.decode(value, {stream:true});
  }
  text += decoder.decode();
  let p;
  try {p = validateCase(JSON.parse(text));} catch (e) {return reply(400, e.message);}
  if (!env.DB) return reply(503, 'case storage unavailable');
  const today = new Date().toISOString().slice(0, 10);
  const perDay = Number(env.MAX_DEFROSTS_PER_DAY) || 500;
  const total = await env.DB.prepare('SELECT COUNT(*) AS n FROM defrost_cases WHERE last_seen = ?').bind(today).first('n');
  const own = await env.DB.prepare('SELECT COUNT(*) AS n FROM defrost_cases WHERE install_id = ? AND last_seen = ?').bind(p.install_id, today).first('n');
  if (total >= perDay || own >= 20) return reply(429, 'daily case limit');
  await env.DB.prepare(`INSERT INTO defrost_cases (install_id, boot_id, event_id, model, observation, report, first_seen, last_seen)
    VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?7)
    ON CONFLICT (install_id, boot_id, event_id) DO UPDATE SET
      observation=excluded.observation, report=excluded.report, last_seen=excluded.last_seen`)
    .bind(p.install_id, p.boot_id, p.event.id, p.model, p.event.observation, JSON.stringify(p), today).run();
  return reply(200);
}
