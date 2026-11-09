// Receives the daily heartbeat of ESPAltherma boards (POST /api/heartbeat), sent once a day by the boards whose owner
// left the report box ticked, from 2.0.0-beta.27. The payload is built by heartbeatPayload() in include/webserver.h:
//   {"v":1,"install_id":"<16 hex>","fw":"2.0.0-beta.27","board":"esp32","model":"<definition>","protocol":"I",
//    "uptime_s":86400,"iu_sw_id":"20002203","iu_model":"EHBX08EF6V",
//    "features":{"dhw_target":"ready","lwt_target":"off","power_limit":"unsupported","meter":"ready","diii":"off","stats":true}}
// One row per board and day: a board sending twice the same day replaces its row. Location as for the reports:
// the country Cloudflare locates the sender in, never the IP. Nothing is read back: {"ok":true} or the invalid field.
// Limits: 2 KB per heartbeat, MAX_ROWS_PER_DAY rows a day.

const MAX_BODY = 2048;
const MAX_ROWS_PER_DAY = 2000; // a few hundred boards a day; the env var MAX_BEATS_PER_DAY overrides it (tests)
const SHORT = /^[\x20-\x7E]{0,32}$/;
const NAME = /^[\x20-\x7E]{1,80}$/;
const FEATURES = ["dhw_target", "lwt_target", "power_limit", "meter", "diii"];
const STATUS = /^[a-z_]{1,16}$/;

const reply = (status, body) => new Response(JSON.stringify(body), {status, headers: {"content-type": "application/json"}});
const bad = error => reply(400, {ok: false, error});

function validate(p) {
  if (!p || typeof p !== "object") return "not a JSON object";
  if (p.v !== 1) return "v";
  if (typeof p.install_id !== "string" || !/^[0-9a-f]{16}$/.test(p.install_id)) return "install_id";
  if (typeof p.fw !== "string" || !SHORT.test(p.fw) || typeof p.board !== "string" || !SHORT.test(p.board)) return "fw/board";
  if (typeof p.model !== "string" || !NAME.test(p.model)) return "model";
  if (p.protocol !== "I" && p.protocol !== "S") return "protocol";
  if (!Number.isInteger(p.uptime_s) || p.uptime_s < 0 || p.uptime_s > 1e9) return "uptime_s";
  if (p.iu_sw_id !== undefined && (typeof p.iu_sw_id !== "string" || !/^[0-9A-F]{4,8}$/.test(p.iu_sw_id))) return "iu_sw_id";
  if (p.iu_model !== undefined && (typeof p.iu_model !== "string" || !/^[A-Z0-9/-]{4,16}$/.test(p.iu_model))) return "iu_model";
  const f = p.features;
  if (!f || typeof f !== "object") return "features";
  for (const k of Object.keys(f)) {
    if (k === "stats") { if (typeof f[k] !== "boolean") return "features.stats"; continue; }
    if (!FEATURES.includes(k) || typeof f[k] !== "string" || !STATUS.test(f[k])) return "features." + k;
  }
  return null;
}

export const onRequestGet = () => new Response(JSON.stringify({ok: false, error: "POST only"}),
  {status: 405, headers: {"content-type": "application/json", "allow": "POST"}});

export async function onRequestPost({request, env}) {
  if (!(request.headers.get("content-type") || "").toLowerCase().startsWith("application/json"))
    return reply(415, {ok: false, error: "content type"});
  const length = Number(request.headers.get("content-length") || 0);
  if (length > MAX_BODY) return reply(413, {ok: false, error: "too large"});
  const text = await request.text();
  if (text.length > MAX_BODY) return reply(413, {ok: false, error: "too large"});
  let p;
  try {
    p = JSON.parse(text);
  } catch (e) {
    return bad("not JSON");
  }
  const error = validate(p);
  if (error) return bad(error);

  const today = new Date().toISOString().slice(0, 10);
  const cf = request.cf || {};
  const country = typeof cf.country === "string" ? cf.country.slice(0, 2) : null;
  const perDay = Number(env.MAX_BEATS_PER_DAY) || MAX_ROWS_PER_DAY;
  const written = await env.DB.prepare("SELECT COUNT(*) AS n FROM heartbeats WHERE day = ?").bind(today).first("n");
  if (written >= perDay) return reply(429, {ok: false, error: "too many heartbeats today"});

  await env.DB.prepare(
    `INSERT INTO heartbeats (install_id, day, fw, board, model, protocol, sw_id, unit_model, uptime, features, country)
     VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11)
     ON CONFLICT (install_id, day) DO UPDATE SET
       fw = excluded.fw, board = excluded.board, model = excluded.model, protocol = excluded.protocol, sw_id = excluded.sw_id,
       unit_model = excluded.unit_model, uptime = excluded.uptime, features = excluded.features, country = excluded.country`
  ).bind(p.install_id, today, p.fw, p.board, p.model, p.protocol, p.iu_sw_id || null, p.iu_model || null, p.uptime_s,
         JSON.stringify(p.features), country).run();
  return reply(200, {ok: true});
}
