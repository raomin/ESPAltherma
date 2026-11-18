// The board of the demo: there is none. This script stands where the board would be and answers the page what a
// real one answered (recordings made with scripts/record_demo.py), replayed in a loop. It is added to the page by
// scripts/build_demo.py only: the page the firmware carries has none of it.
//
// The recordings are scenes of the same board, one per thing a heat pump does (standing by, hot water, heating),
// each taken while the unit is fully at it and played in a loop at the speed of the clock: the visitor picks one
// in the banner, and ?scene=<id> in the address opens the demo on it.
//
// The page reaches a board through fetch() (/api/...), one EventSource (/events) and one XMLHttpRequest (the
// firmware upload): the three are replaced here, before the page's own script runs. Settings, targets and the
// selection of values are kept in memory and behave as on a board; a reload starts again from the recording.
(() => {
  'use strict';
  const REC = /*RECORDING*/null, VERSION = /*VERSION*/'';
  const clone = x => JSON.parse(JSON.stringify(x));
  const A = REC.answers, scenes = REC.scenes;
  const now = () => Math.floor(Date.now() / 1000);

  // A status of the stream is written as what changed since the one before (build_demo.py): made whole again here
  const patched = (base, patch) => {
    if (!patch || typeof patch !== 'object' || Array.isArray(patch) || !base || typeof base !== 'object' || Array.isArray(base)) return patch;
    const out = {...base};
    for (const k of Object.keys(patch)) out[k] = patched(base[k], patch[k]);
    return out;
  };
  // The values likewise: the whole list the first time, then {place in the list: new text}
  for (const s of scenes) {
    let last = null, list = null;
    for (const e of s.stream) {
      if (e.event === 'status') last = e.data = e.full ? e.data : patched(last, e.data);
      if (e.event === 'values') list = e.data = Array.isArray(e.data) ? e.data : list.map((x, i) => i in e.data ? {...x, v: e.data[i]} : x);
    }
    s.span = s.stream.length ? s.stream[s.stream.length - 1].at + 5 : 60; // it starts again after its end
    s.first = kind => (s.stream.find(e => e.event === kind) || {}).data;
  }
  let scene = scenes.find(s => s.id === new URLSearchParams(location.search).get('scene')) ||
    scenes.find(s => s.id === REC.opens) || scenes[0];

  // ---- the board's state
  const cfg = clone(A['/api/config']);
  const catalog = A['/api/catalog'] || [];
  const names = REC.names || {};                     // {lang: {label: name}}, the catalog's translated names
  const firstStatus = A['/api/status'];
  const fw = VERSION || firstStatus.fw;
  const ctl = clone(firstStatus.control || {});      // the targets and the power limit, as the board reports them
  const current = {values: scene.first('values') || A['/api/values'] || [], status: scene.first('status') || firstStatus};
  const added = [];                                  // events of this visit, on top of the recorded history
  const sources = new Set();                         // the open event streams
  let boot = (A['/api/events'] || {}).boot || 1, bootAt = Date.now() - firstStatus.uptime * 1000, downUntil = 0;
  let restartCause = firstStatus.restart_cause || '', valuesAt = Date.now() - (firstStatus.hp.last_poll || 0) * 1000;
  let surveyUntil = 0, discoverUntil = 0, discoverDone = false, mqttTestUntil = 0, mqttTested = false, scanUntil = 0;
  let memory = clone(A['/api/memory'] || {busy: false, done: false, ok: false, addr: '0x0000', len: 0, hex: ''});
  let dump = null;

  const down = () => Date.now() < downUntil;
  const uptime = () => Math.max(0, Math.floor((Date.now() - bootAt) / 1000));
  const event = text => added.push({t: now(), up: uptime(), boot, text});

  // A restart: the board is away for a few seconds, its streams close, and it comes back with a new start
  function restart(cause, seconds = 6) {
    downUntil = Date.now() + seconds * 1000;
    bootAt = downUntil;
    boot++;
    restartCause = cause;
    for (const s of [...sources]) s.close();
    added.push({t: now() + seconds, up: 0, boot, text: 'Boot #' + boot + ': restart by the firmware, firmware ' + fw});
  }

  const selected = c => cfg.hp.labels && cfg.hp.labels.length ? cfg.hp.labels.includes(c.k) : c.sel;
  function values() {
    if (!cfg.hp.model) return [];
    const read = {};
    for (const v of current.values) read[v.l] = v.v;
    // a value the recording did not follow keeps the reading the catalog had of it
    return catalog.filter(selected).map(c => ({l: c.l, v: c.l in read ? read[c.l] : c.v, r: c.r}));
  }

  // The targets: a request is applied two seconds later, as the heat pump task does at its next turn
  function controls() {
    const out = {}, at = Date.now();
    for (const k of ['dhw', 'lwt']) {
      const c = ctl[k];
      if (!c || !cfg.control[k + '_target']) continue;
      if (c.due && at >= c.due) {
        if (c.restore) { c.target = c.orig_hi; c.target_lo = c.orig_lo; c.held = -1; c.saved = c.orig_hi; }
        else {
          c.target = c.target_lo = c.pending;
          if (cfg.control.ram_only) c.held = c.pending; else c.saved = c.pending;
        }
        c.pending = -1; c.restoring = false; c.due = 0; c.restore = false;
      }
      const shown = clone(c);
      delete shown.due; delete shown.restore; delete shown.shift;
      shown.ram_only = !!cfg.control.ram_only;
      shown.effective = c.ceiling >= 0 ? Math.min(c.target + c.shift, c.ceiling) : c.target + c.shift;
      out[k] = shown;
    }
    const l = ctl.limit;
    if (l && cfg.control.power_limit) {
      if (l.due && at >= l.due) {
        if (l.next === 'off') { l.on = false; l.value = -1; l.enable = 0; l.active = l.rest; }
        else { l.on = true; l.value = l.limit = l.active = l.next; l.enable = 1; }
        l.due = 0;
      }
      const shown = clone(l);
      delete shown.due; delete shown.next; delete shown.rest;
      out.limit = shown;
    }
    return out;
  }
  for (const k of ['dhw', 'lwt']) if (ctl[k]) ctl[k].shift = ctl[k].ceiling >= 0 ? 0 : ctl[k].effective - ctl[k].target;
  if (ctl.limit) ctl.limit.rest = ctl.limit.active; // what the unit applies with no limit from here

  function status() {
    const s = clone(current.status), c = cfg;
    s.fw = fw;
    s.uptime = uptime();
    s.hostname = c.hostname;
    s.restart_cause = restartCause;
    s.wifi.ssid = c.wifi.ssid;
    s.mqtt.off = !!c.mqtt.off;
    s.mqtt.configured = !!c.mqtt.server && !c.mqtt.off;
    s.mqtt.connected = s.mqtt.configured;
    s.mqtt.server = c.mqtt.server;
    s.update = {check: !(c.update && c.update.check === false), latest: fw, available: false, checked: Math.min(s.uptime, 180)};
    s.hp.protocol = c.hp.protocol; s.hp.model = c.hp.model; s.hp.confirmed = c.hp.confirmed;
    s.hp.values = values().length;
    s.hp.last_poll = c.hp.model ? Math.floor((Date.now() - valuesAt) / 1000) : -1;
    s.hp.survey_running = Date.now() < surveyUntil; s.hp.survey_done = !s.hp.survey_running;
    const relays = {}, r = c.relays, was = current.status.relays || {};
    if (r.therm_pin >= 0) relays.thermostat = !!was.thermostat;
    if (r.cool_pin >= 0) relays.cooling = !!was.cooling;
    if (r.sg1_pin >= 0 && r.sg2_pin >= 0) relays.sg = was.sg || 0;
    if (r.safety_pin >= 0) relays.safety = !!was.safety;
    s.relays = relays;
    if (!c.stats.enabled) delete s.stats; else if (s.stats) s.stats.tz = c.stats.tz;
    const k = controls();
    if (Object.keys(k).length) s.control = k; else delete s.control;
    return s;
  }

  function eventHistory() {
    const h = clone(A['/api/events'] || {events: []}), shift = now() - REC.recorded;
    for (const e of h.events) if (e.t) e.t += shift;     // the recorded history, as if it had just happened
    h.events = added.filter(e => e.t <= now()).reverse().concat(h.events).slice(0, 24); // a board keeps 24
    h.boot = boot; h.uptime = uptime(); h.clock = true;
    return h;
  }

  // ---- settings
  function save(d) {
    const pins = r => [r.therm_pin, r.cool_pin, r.sg1_pin, r.sg2_pin, r.safety_pin].join();
    const before = [cfg.hp.rx, cfg.hp.tx, pins(cfg.relays), cfg.hostname, cfg.admin.has_pwd].join();
    for (const [name, value] of Object.entries(d)) {
      if (!value || typeof value !== 'object' || Array.isArray(value)) { cfg[name] = value; continue; }
      const section = cfg[name] = cfg[name] || {};
      for (const [k, v] of Object.entries(value)) {
        if (k === 'pwd') section.has_pwd = !!v;        // a password is never kept, only that there is one
        else if (v !== null) section[k] = v;
      }
    }
    if (d.hp && 'model' in d.hp && !('layout_fixes' in d.hp)) cfg.hp.layout_fixes = cfg.hp.layout_fixes || [];
    const reboot = before !== [cfg.hp.rx, cfg.hp.tx, pins(cfg.relays), cfg.hostname, cfg.admin.has_pwd].join();
    if (reboot) restart('Settings changed from the web interface');
    return {ok: true, reboot};
  }

  function control(d) {
    const refuse = (code, error) => ({code, body: {ok: false, error}});
    if (typeof d.power_limit === 'number' || typeof d.power_limit_on === 'boolean') {
      const l = ctl.limit;
      if (!l || !cfg.control.power_limit) return refuse(409, 'not ready');
      if (typeof d.power_limit_on === 'boolean') l.next = d.power_limit_on ? (l.value > 0 ? l.value : l.limit) : 'off';
      else if (d.power_limit < l.min || d.power_limit > l.max) return refuse(400, 'out of range');
      else l.next = d.power_limit;
      l.due = Date.now() + 2000;
      return {ok: true};
    }
    const k = 'lwt_target' in d || d.lwt_restore === true ? 'lwt' : 'dhw_target' in d || d.dhw_restore === true ? 'dhw' : null;
    if (!k) return refuse(400, 'dhw_target or lwt_target missing');
    const c = ctl[k], name = k === 'lwt' ? 'Leaving water target' : 'Hot water target';
    if (!c || !cfg.control[k + '_target']) return refuse(409, 'not ready');
    if (d[k + '_restore'] === true) {
      c.restore = c.restoring = true; c.due = Date.now() + 2000;
      event(name + ": the unit's curve asked back from the web page (demo)");
      return {ok: true};
    }
    const v = d[k + '_target'];
    if (!Number.isInteger(v) || v < c.min || v > c.max) return refuse(400, 'out of range');
    c.pending = v; c.restore = false; c.due = Date.now() + 2000;
    event(name + ' set to ' + v);
    return {ok: true};
  }

  // ---- the routes, as include/webserver.h has them
  const GET = {
    '/api/status': status,
    '/api/config': () => cfg,
    '/api/values': values,
    '/api/models': () => A['/api/models'] || [],
    '/api/catalog': q => {
      const read = {}, n = names[q.get('lang')] || {};
      for (const v of current.values) read[v.l] = v.v;
      return catalog.map(c => ({...c, sel: selected(c), v: c.l in read ? read[c.l] : c.v, ...(n[c.l] ? {n: n[c.l]} : {})}));
    },
    '/api/events': eventHistory,
    '/api/detect': () => A['/api/detect'] || {},
    '/api/log': () => ({text: A['/api/log'] || ''}),
    '/api/heartbeat': () => ({...(A['/api/heartbeat'] || {}), fw, uptime_s: uptime()}),
    '/api/telemetry': () => A['/api/telemetry'] || {},
    '/api/memory': () => memory,
    '/api/memory/dump/status': () => {
      if (!dump) return {active: false, done: false, from: 0, len: 0, pos: 0, missing: 0};
      const pos = Math.min(dump.len, Math.floor((Date.now() - dump.at) / 1000 * 600));
      return {active: pos < dump.len, done: pos >= dump.len, from: dump.from, len: dump.len, pos, missing: 0};
    },
    '/api/wifi/scan': () => {
      if (!scanUntil) scanUntil = Date.now() + 2500;
      if (Date.now() < scanUntil) return {running: true};
      scanUntil = 0;
      return {running: false, networks: [{ssid: cfg.wifi.ssid || 'HomeWiFi', rssi: -52, secure: true}, {ssid: 'Neighbour', rssi: -81, secure: true}]};
    },
    '/api/discover': () => {
      const running = Date.now() < discoverUntil;
      return {running, done: discoverDone && !running, hosts: discoverDone && !running ? [{name: 'homeassistant', ip: '192.168.1.10', port: 1883, ha: true, mqtt: true}] : []};
    },
    '/api/mqtt/test': () => ({result: Date.now() < mqttTestUntil ? 'running' : mqttTested ? 'ok' : ''}),
  };
  const POST = {
    '/api/config': (q, d) => save(d),
    '/api/control': (q, d) => control(d),
    '/api/detect': () => { surveyUntil = Date.now() + 5000; return {ok: true}; },
    '/api/discover': () => { discoverUntil = Date.now() + 3000; discoverDone = true; return {ok: true}; },
    '/api/mqtt/test': () => { mqttTestUntil = Date.now() + 2000; mqttTested = true; return {ok: true}; },
    '/api/memory': (q, d) => {
      const addr = parseInt(d.addr, 16) || 0, len = Math.min(64, Math.max(1, d.len || 16));
      let hex = '';
      for (let i = 0; i < len; i++) hex += ((addr * 7 + i * 13) & 0xFF).toString(16).padStart(2, '0').toUpperCase(); // not a unit's memory
      memory = {busy: false, done: true, ok: true, addr: '0x' + addr.toString(16).toUpperCase().padStart(4, '0'), len, hex};
      return {ok: true};
    },
    '/api/memory/dump': (q, d) => {
      const from = parseInt(d.from, 16) || 0, to = parseInt(d.to, 16) || 0;
      if (to < from || to - from >= 16384) return {code: 400, body: {ok: false, error: 'Not available on this unit.'}};
      dump = {from, len: to - from + 1, at: Date.now()};
      return {ok: true};
    },
    '/api/reboot': () => { restart('Restart requested from the web interface'); return {ok: true}; },
    '/api/factory-reset': () => ({ok: true}), // the demo keeps its settings: a reload gives them back anyway
  };

  const boardPath = p => p.startsWith('/api/') || p === '/events' || p === '/update';
  const realFetch = window.fetch.bind(window);
  window.fetch = (input, opts = {}) => {
    const url = new URL(typeof input === 'string' ? input : input.url, location.href);
    // The update check reads the installer site: here it finds the version the demo runs, so nothing is offered
    if (/\/manifest-[^/]+\.json$/.test(url.pathname) && !boardPath(url.pathname))
      return Promise.resolve(new Response(JSON.stringify({version: fw}), {headers: {'content-type': 'application/json'}}));
    if (url.origin !== location.origin || !boardPath(url.pathname)) return realFetch(input, opts);
    return new Promise((resolve, reject) => setTimeout(() => {
      if (down()) return reject(new TypeError('Failed to fetch'));
      const method = (opts.method || 'GET').toUpperCase(), route = (method === 'POST' ? POST : GET)[url.pathname];
      if (!route) return resolve(new Response('{"ok":false,"error":"not found"}', {status: 404, headers: {'content-type': 'application/json'}}));
      let body = {};
      try { body = opts.body ? JSON.parse(opts.body) : {}; } catch (e) {}
      const r = route(url.searchParams, body);
      if (url.pathname === '/api/log') return resolve(new Response(r.text, {headers: {'content-type': 'text/plain'}}));
      resolve(new Response(JSON.stringify(r.code ? r.body : r), {status: r.code || 200, headers: {'content-type': 'application/json'}}));
    }, 60));
  };

  // ---- the event stream: what the recording sent, at the pace it sent it
  class Events {
    constructor() {
      this.readyState = Events.CONNECTING;
      this.handlers = {};
      setTimeout(() => {
        if (this.readyState === Events.CLOSED) return;
        if (down()) return this.close();
        this.readyState = Events.OPEN;
        sources.add(this);
        this.send('status', status());
        this.send('values', values());
      }, 80);
    }
    addEventListener(type, fn) { (this.handlers[type] = this.handlers[type] || []).push(fn); }
    removeEventListener(type, fn) { this.handlers[type] = (this.handlers[type] || []).filter(f => f !== fn); }
    send(type, data) { for (const fn of this.handlers[type] || []) fn({type, data: typeof data === 'string' ? data : JSON.stringify(data)}); }
    close() { this.readyState = Events.CLOSED; sources.delete(this); }
  }
  Events.CONNECTING = 0; Events.OPEN = 1; Events.CLOSED = 2;
  window.EventSource = Events;

  // The scene plays in a loop from its start, at the speed of the clock (pos: seconds into its recording). Another
  // scene starts at its own start, where the unit is fully at it, and the open pages get that state at once
  let pos = 0, next = 0, tickAt = Date.now();
  function play(s) {
    if (s === scene) return;
    scene = s; pos = 0; next = 0; tickAt = Date.now();
    current.values = scene.first('values') || current.values;
    current.status = scene.first('status') || current.status;
    valuesAt = Date.now();
    for (const src of sources) { src.send('status', status()); src.send('values', values()); }
  }
  setInterval(() => {
    const t = Date.now(), passed = Math.min(2, (t - tickAt) / 1000); // a tab left asleep: no catching up
    tickAt = t;
    const stream = scene.stream;
    if (down() || !stream.length) return;
    pos += passed;
    for (;;) {
      if (next >= stream.length) {
        if (pos < scene.span) break;
        pos -= scene.span; next = 0;
      }
      const e = stream[next];
      if (e.at > pos) break;
      next++;
      if (e.event === 'values') { current.values = e.data; valuesAt = Date.now(); }
      if (e.event === 'status') current.status = e.data;
      const data = e.event === 'values' ? values() : e.event === 'status' ? status() : e.data;
      for (const s of sources) s.send(e.event, data);
    }
  }, 250);

  // ---- the firmware upload: received, thrown away, and the board "restarts" on the version it had. Only a request
  // opened on the board's /update is answered here: any other is the browser's own (the visit counter the site
  // adds to its pages sends its figures with one)
  const RealRequest = window.XMLHttpRequest;
  window.XMLHttpRequest = function () {
    const x = new RealRequest(), open = x.open.bind(x);
    x.open = (method, address, ...rest) => {
      const url = new URL(address, location.href);
      if (url.origin !== location.origin || url.pathname !== '/update') return open(method, address, ...rest);
      x.send = () => {
        let sent = 0;
        const timer = setInterval(() => {
          sent += 8;
          if (x.upload.onprogress) x.upload.onprogress({loaded: Math.min(sent, 100), total: 100});
          if (sent < 100) return;
          clearInterval(timer);
          Object.defineProperty(x, 'status', {value: 200});
          Object.defineProperty(x, 'responseText', {value: '{"ok":true}'});
          restart('Firmware update from the web interface (demo: nothing was installed)', 8);
          if (x.onload) x.onload();
        }, 250);
      };
    };
    return x;
  };
  window.XMLHttpRequest.prototype = RealRequest.prototype;
  for (const k of ['UNSENT', 'OPENED', 'HEADERS_RECEIVED', 'LOADING', 'DONE']) window.XMLHttpRequest[k] = RealRequest[k];

  // ---- the two things a page of a site cannot do like a board
  document.addEventListener('click', e => {
    const b = e.target.closest && e.target.closest('#dumpGet');
    if (!b) return;
    e.stopImmediatePropagation(); e.preventDefault();  // the board sends a file; here the page would leave for a 404
    const a = document.createElement('a');
    a.href = URL.createObjectURL(new Blob(['ESPAltherma demo: no heat pump was read.\n'], {type: 'text/plain'}));
    a.download = 'espaltherma-memory-dump.txt';
    a.click();
  }, true);
  document.addEventListener('DOMContentLoaded', () => {
    // Said on every page, in the page's own banner: above the pages, under the menu
    const bar = document.createElement('div');
    bar.className = 'banner info';
    bar.innerHTML = '<span style="flex:1 1 320px"><b>Demo.</b> A real heat pump, recorded and replayed in a loop. ' +
      'Nothing here reaches a board: change what you like, a reload puts it back.</span>' +
      '<a class="btn" href="../">Install ESPAltherma</a>' +
      '<div id="demoScenes" style="flex:1 1 100%;display:flex;flex-wrap:wrap;gap:8px;align-items:center"><span>Show it:</span></div>';
    const row = bar.querySelector('#demoScenes');
    const mark = () => { for (const b of row.querySelectorAll('button')) b.className = b.dataset.scene === scene.id ? 'p' : 'btn'; };
    for (const s of scenes) {
      const b = document.createElement('button');
      b.dataset.scene = s.id;
      b.textContent = s.title;
      b.onclick = () => {
        play(s); mark();
        const u = new URL(location.href);
        u.searchParams.set('scene', s.id);
        history.replaceState(null, '', u);           // the address opens the demo on this scene
      };
      row.append(b);
    }
    mark();
    if (scenes.length < 2) row.remove();
    document.querySelector('main').prepend(bar);
  });
})();
