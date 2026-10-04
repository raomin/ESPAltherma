// Receives the opt-in heat pump reports of ESPAltherma (POST /api/report), sent once when the owner confirms the model.
// The payload is built by telemetryPayload() in include/webserver.h:
//   {"install_id":"<16 hex>","model":"<definition>","confirmed":true,"layout_fixes":[{"reg":33,"model":"..."}],"report":{survey}}
// One row per board, identification key and model: a board sending again updates its row.
// Location: the country and city Cloudflare locates the sender in (request.cf). The IP address itself is never stored.
// Nothing is ever read back: the answer is {"ok":true} or the name of the invalid field.
// Anyone can post, so the database only holds suggestions: a report is checked before its key goes into
// data/fingerprints.json. Limits keep junk bounded: 8 KB per report, 20 rows per board, MAX_ROWS_PER_DAY a day.

const MAX_BODY = 8192;
const MAX_ROWS_PER_INSTALL = 20;
const MAX_ROWS_PER_DAY = 500; // real reports are a few a day; the env var MAX_ROWS_PER_DAY overrides it (tests)
const KEY_I = /^I\|63:[0-9A-F-]{12}\|60:[0-9A-F-]{4}\|CAP:[0-9A-F-]{2}\|11:[0-9A-F-]{12}\|00:[0-9A-F-]{4}$/i;
const KEY_S = /^S\|50:[01]\|56:[01]$/;
const NAME = /^[\x20-\x7E]{1,80}$/; // definition names are printable ASCII
const SHORT = /^[\x20-\x7E]{0,32}$/;

const reply = (status, body) => new Response(JSON.stringify(body), {status, headers: {"content-type": "application/json"}});
const bad = error => reply(400, {ok: false, error});

function validate(p) {
  if (!p || typeof p !== "object") return "not a JSON object";
  if (typeof p.install_id !== "string" || !/^[0-9a-f]{16}$/.test(p.install_id)) return "install_id";
  if (typeof p.model !== "string" || !NAME.test(p.model)) return "model";
  if (typeof p.confirmed !== "boolean") return "confirmed";
  if (!Array.isArray(p.layout_fixes) || p.layout_fixes.length > 4) return "layout_fixes";
  for (const f of p.layout_fixes)
    if (!f || !Number.isInteger(f.reg) || f.reg < 0 || f.reg > 255 || typeof f.model !== "string" || !NAME.test(f.model)) return "layout_fixes";
  const r = p.report;
  if (!r || typeof r !== "object") return "report";
  if (typeof r.key !== "string" || !(KEY_I.test(r.key) || KEY_S.test(r.key))) return "report.key";
  if (r.protocol !== "I" && r.protocol !== "S") return "report.protocol";
  if (typeof r.fw !== "string" || !SHORT.test(r.fw) || typeof r.board !== "string" || !SHORT.test(r.board)) return "report.fw/board";
  return null;
}

// Only POST: without this, Pages would answer the other methods with the static page
export const onRequestGet = () => new Response(JSON.stringify({ok: false, error: "POST only"}),
  {status: 405, headers: {"content-type": "application/json", "allow": "POST"}});

export async function onRequestPost({request, env}) {
  // The firmware sends application/json. Other types are what a web page can make a visitor's browser post.
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

  const today = new Date().toISOString().slice(0, 10); // the day only
  const cf = request.cf || {};
  const country = typeof cf.country === "string" ? cf.country.slice(0, 2) : null;
  const city = typeof cf.city === "string" ? cf.city.slice(0, 80) : null;
  const perDay = Number(env.MAX_ROWS_PER_DAY) || MAX_ROWS_PER_DAY;
  const written = await env.DB.prepare("SELECT COUNT(*) AS n FROM reports WHERE last_seen = ?").bind(today).first("n");
  if (written >= perDay) return reply(429, {ok: false, error: "too many reports today, try again tomorrow"});
  const rows = await env.DB.prepare("SELECT COUNT(*) AS n FROM reports WHERE install_id = ?").bind(p.install_id).first("n");
  if (rows >= MAX_ROWS_PER_INSTALL) return reply(429, {ok: false, error: "too many reports from this board"});

  await env.DB.prepare(
    `INSERT INTO reports (install_id, key, model, confirmed, fw, board, protocol, fixes, report, country, city, first_seen, last_seen, count)
     VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?12, 1)
     ON CONFLICT (install_id, key, model) DO UPDATE SET
       confirmed = excluded.confirmed, fw = excluded.fw, board = excluded.board, fixes = excluded.fixes,
       report = excluded.report, country = excluded.country, city = excluded.city,
       last_seen = excluded.last_seen, count = reports.count + 1`
  ).bind(p.install_id, p.report.key, p.model, p.confirmed ? 1 : 0, p.report.fw, p.report.board, p.report.protocol,
         JSON.stringify(p.layout_fixes), JSON.stringify(p.report), country, city, today).run();
  return reply(200, {ok: true});
}
