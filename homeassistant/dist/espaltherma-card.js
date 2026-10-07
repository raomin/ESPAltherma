// ESPAltherma card and dashboard for Home Assistant: the live drawing of the heating installation of the
// ESPAltherma web page, fed by the entities ESPAltherma publishes through MQTT discovery.
//   card:      type: custom:espaltherma-card   (options: climate, entities: {key: entity_id})
//   dashboard: strategy: {type: custom:espaltherma}   (options: climate)
const VERSION = "1.0.0";

// Entities, by the ids ESPAltherma's discovery gives them (built from the English labels, the same everywhere)
const FIND = {
  out: /^sensor\.espaltherma_.*outdoor_air_temp/, lw1: /^sensor\.espaltherma_leaving_water_temp_before_buh/,
  lw2: /^sensor\.espaltherma_leaving_water_temp_after_buh/, r3t: /^sensor\.espaltherma_refrig_temp_liquid_side/,
  inlet: /^sensor\.espaltherma_inlet_water_temp/, tank: /^sensor\.espaltherma_dhw_tank_temp/,
  room: /^sensor\.espaltherma_indoor_ambient_temp/, dhwSp: /^sensor\.espaltherma_dhw_setpoint$/,
  lwSp: /^sensor\.espaltherma_lw_setpoint_main/, rtSp: /^sensor\.espaltherma_rt_setpoint$/,
  inv: /^sensor\.espaltherma_inv_frequency/, amp: /^sensor\.espaltherma_inv_primary_current/,
  v3: /^binary_sensor\.espaltherma_3way_valve/, pump: /^binary_sensor\.espaltherma_water_pump_operation/,
  flow: /^sensor\.espaltherma_flow_sensor/, bar: /^sensor\.espaltherma_water_pressure$/,
  mode: /^sensor\.espaltherma_operation_mode$/, err: /^sensor\.espaltherma_error_type$/,
  defrost: /^binary_sensor\.espaltherma_defrost_operation$/,
};

const I18N = {"fr":{"3-way valve":"Vanne 3 voies","Backup heater":"Appoint électrique","Compressor":"Compresseur","Flow":"Débit","Heat exchanger":"Échangeur","Heat pump out":"Sortie PAC","Hot water":"Eau chaude","Indoor unit":"Unité intérieure","Leaving":"Départ","Living room":"Séjour","Outdoor unit":"Unité extérieure","Outside":"Extérieur","Pump off":"Pompe arrêtée","Pump on":"Pompe en marche","Refrigerant":"Réfrigérant","Resting":"Au repos","Return":"Retour","Water temperature":"Température de l'eau","target {n}°":"consigne {n}°","to the hot water":"vers l'eau chaude","to the house":"vers la maison"},"de":{"3-way valve":"3-Wege-Ventil","Backup heater":"Heizstab","Compressor":"Verdichter","Flow":"Durchfluss","Heat exchanger":"Wärmetauscher","Heat pump out":"WP-Austritt","Hot water":"Warmwasser","Indoor unit":"Innengerät","Leaving":"Vorlauf","Living room":"Wohnzimmer","Outdoor unit":"Außengerät","Outside":"Außen","Pump off":"Pumpe aus","Pump on":"Pumpe an","Refrigerant":"Kältemittel","Resting":"In Ruhe","Return":"Rücklauf","Water temperature":"Wassertemperatur","target {n}°":"Soll {n}°","to the hot water":"zum Warmwasser","to the house":"zum Haus"},"it":{"3-way valve":"Valvola a 3 vie","Backup heater":"Resistenza elettrica","Compressor":"Compressore","Flow":"Portata","Heat exchanger":"Scambiatore","Heat pump out":"Uscita PdC","Hot water":"Acqua calda","Indoor unit":"Unità interna","Leaving":"Mandata","Living room":"Soggiorno","Outdoor unit":"Unità esterna","Outside":"Esterno","Pump off":"Pompa spenta","Pump on":"Pompa accesa","Refrigerant":"Refrigerante","Resting":"A riposo","Return":"Ritorno","Water temperature":"Temperatura dell'acqua","target {n}°":"obiettivo {n}°","to the hot water":"verso l'acqua calda","to the house":"verso la casa"},"es":{"3-way valve":"Válvula de 3 vías","Backup heater":"Resistencia de apoyo","Compressor":"Compresor","Flow":"Caudal","Heat exchanger":"Intercambiador","Heat pump out":"Salida BdC","Hot water":"Agua caliente","Indoor unit":"Unidad interior","Leaving":"Impulsión","Living room":"Salón","Outdoor unit":"Unidad exterior","Outside":"Exterior","Pump off":"Bomba parada","Pump on":"Bomba en marcha","Refrigerant":"Refrigerante","Resting":"En reposo","Return":"Retorno","Water temperature":"Temperatura del agua","target {n}°":"consigna {n}°","to the hot water":"hacia el agua caliente","to the house":"hacia la casa"}};

// Shrinks an SVG text to max units wide (translations can be longer than the English the drawing was made for)
const fit = (el, max) => { el.style.fontSize = ''; const w = el.getComputedTextLength ? el.getComputedTextLength() : 0;
  if (w > max) el.style.fontSize = (parseFloat(getComputedStyle(el).fontSize) * max / w).toFixed(1) + 'px'; };

function tr(hass, s, p) {
  const lang = ((hass && (hass.locale && hass.locale.language || hass.language)) || "en").slice(0, 2);
  let r = (I18N[lang] && I18N[lang][s]) || s;
  if (p) r = r.replace(/\{(\w+)\}/g, (m, k) => (p[k] != null ? p[k] : m));
  return r;
}

function findEntities(hass, config) {
  const ids = Object.keys(hass.states);
  const e = {};
  for (const [k, re] of Object.entries(FIND)) e[k] = ids.find(id => re.test(id));
  Object.assign(e, (config && config.entities) || {});
  return e;
}

function defaultClimate(hass) {
  if (hass.states["climate.altherma"]) return "climate.altherma";
  return Object.keys(hass.states).find(id => id.startsWith("climate.") && /altherma|heat_?pump|daikin/i.test(id));
}

// Water colour: blue (15°) to red (55°), the scale of the ESPAltherma web page
const SCALE = {light: ["#2e86de", "#737fb9", "#9e7494", "#c1626c", "#e0433a"], dark: ["#4f9cf0", "#8995ce", "#b18aaa", "#d37b85", "#f2645a"]};
function tcol(v, dark) {
  if (v == null || isNaN(v)) return "var(--divider-color)";
  const s = SCALE[dark ? "dark" : "light"].map(h => [1, 3, 5].map(j => parseInt(h.slice(j, j + 2), 16)));
  const x = Math.max(0, Math.min(1, (v - 15) / 40)) * 4, i = Math.min(3, Math.floor(x));
  return "rgb(" + s[i].map((c, j) => Math.round(c + (s[i + 1][j] - c) * (x - i))) + ")";
}

const TAGS = [["Outside", "out", 40, 118, 84], ["Heat pump out", "lw1", 342, 172, 110], ["Leaving", "lw2", 340, 38, 84],
  ["Refrigerant", "r3t", 156, 342, 96], ["Return", "inlet", 312, 418, 84], ["Flow", "flow", 316, 468, 98]];

const STYLE = `
:host{--ea-card:var(--ha-card-background,var(--card-background-color,#fff));--ea-ink:var(--primary-text-color);--ea-ink2:var(--secondary-text-color);
--ea-line:var(--divider-color);--ea-soft:var(--secondary-background-color);--ea-hot:#e0433a;--ea-cold:#2e86de;--ea-refr:#7d8597}
:host([dark]){--ea-hot:#f2645a;--ea-cold:#4f9cf0;--ea-refr:#979dac}
ha-card{padding:8px;overflow:hidden}
.wrap{overflow-x:auto}
svg{display:block;width:100%;height:auto;min-width:480px}
.pipe{fill:none;stroke-width:7;stroke-linecap:round;stroke-linejoin:round;transition:stroke .8s}
.fl{fill:none;stroke:var(--ea-card);stroke-width:2.4;stroke-dasharray:2 16;stroke-linecap:round;opacity:0}
.flowing .act .fl{opacity:.95;animation:fl .9s linear infinite}
@keyframes fl{to{stroke-dashoffset:-18}}
.br{transition:opacity .6s}.br:not(.act){opacity:.28}
.eq{fill:var(--ea-card);stroke:var(--ea-ink2);stroke-width:1.6}
.ln{fill:none;stroke:var(--ea-ink2);stroke-width:1.5}
.area{fill:var(--ea-soft);stroke:none}
.tag rect{fill:var(--ea-card);stroke:var(--ea-line);stroke-width:1.2}
.tag,.click{cursor:pointer}
.tl{font:600 12px var(--paper-font-body1_-_font-family,sans-serif);fill:var(--ea-ink2)}
.tv{font:600 18px var(--paper-font-body1_-_font-family,sans-serif);fill:var(--ea-ink)}
.tb{font:600 38px var(--paper-font-body1_-_font-family,sans-serif);fill:var(--ea-ink)}
.ts{font:500 13px var(--paper-font-body1_-_font-family,sans-serif);fill:var(--ea-ink2)}
#fan{transform-origin:95px 238px;animation:drift 26s ease-in-out infinite}
.fanon #fan{animation:spin 1.6s linear infinite}
@keyframes drift{0%{transform:rotate(0)}10%{transform:rotate(8deg)}18%{transform:rotate(6deg)}34%{transform:rotate(38deg)}42%{transform:rotate(44deg)}50%{transform:rotate(41deg)}68%{transform:rotate(82deg)}80%{transform:rotate(96deg)}90%{transform:rotate(104deg)}100%{transform:rotate(120deg)}}
@keyframes spin{to{transform:rotate(360deg)}}
.valve path{fill:var(--ea-card);stroke:var(--ea-ink2);stroke-width:1.5}.valve path.shut{fill:var(--ea-ink2)}
.comp #comp{fill:var(--primary-color);stroke:var(--primary-color)}
.sens{fill:var(--ea-ink2)}
@media (prefers-reduced-motion:reduce){.flowing .act .fl,#fan{animation:none!important}}`;

const SVG = `<svg viewBox="0 0 800 560" role="img">
<defs><linearGradient id="lg"><stop offset="0" class="s0"/><stop offset=".25" class="s1"/><stop offset=".5" class="s2"/><stop offset=".75" class="s3"/><stop offset="1" class="s4"/></linearGradient></defs>
<rect class="area" x="190" y="30" width="340" height="515" rx="26"/>
<text class="tl" x="210" y="56" data-t="Indoor unit"></text>
<text class="tl" x="95" y="392" text-anchor="middle" data-t="Outdoor unit"></text>
<rect class="eq" x="20" y="170" width="150" height="190" rx="18"/>
<circle class="ln" cx="95" cy="238" r="48"/>
<g id="fan" style="fill:var(--ea-cold);opacity:.8"><ellipse cx="95" cy="215" rx="8" ry="20"/><ellipse cx="95" cy="215" rx="8" ry="20" transform="rotate(120 95 238)"/><ellipse cx="95" cy="215" rx="8" ry="20" transform="rotate(240 95 238)"/></g>
<circle cx="95" cy="238" r="5" class="eq"/>
<g class="click" data-key="inv"><rect class="eq" x="33" y="310" width="13" height="30" rx="6.5"/><path class="ln" d="M46 331H52"/>
<rect class="eq" id="comp" x="52" y="300" width="30" height="50" rx="12"/>
<text class="tl" x="92" y="318" id="lComp" data-t="Compressor"></text>
<text class="tv" x="92" y="340" id="sInv"></text></g>
<path d="M170 240H230M170 330H230" style="stroke:var(--ea-refr);stroke-width:7;fill:none;stroke-linecap:round"/>
<path d="M170 240H230M170 330H230" style="stroke:var(--ea-card);stroke-width:3;fill:none"/>
<circle class="sens" cx="206" cy="330" r="4.5"/>
<rect class="eq" x="230" y="200" width="40" height="160" rx="8"/>
<path class="ln" d="M234 212l32 12-32 12 32 12-32 12 32 12-32 12 32 12-32 12 32 12-32 12 32 12"/>
<text class="tl" x="250" y="192" text-anchor="middle" data-t="Heat exchanger"></text>
<g id="gMain" class="br act"><path class="pipe" id="pSup" d="M270 220H330V90H470"/><path class="pipe" id="pRet" d="M580 525H300V340H270"/></g>
<rect class="eq" x="318" y="108" width="24" height="54" rx="8"/>
<path class="ln" d="M330 114v4l-6 4 12 6-12 6 12 6-12 6 6 4v6" style="stroke:var(--ea-hot)"/>
<text class="tl" x="310" y="140" text-anchor="end" data-t="Backup heater"></text>
<g class="click" data-key="tank"><rect class="eq" x="590" y="28" width="150" height="236" rx="34"/>
<rect id="tankFill" x="594" y="32" width="142" height="228" rx="31" style="opacity:.2"/></g>
<path class="eq" d="M580 360L685 292L790 360V540H580Z" style="stroke-linejoin:round"/>
<g id="gDhw" class="br"><path class="pipe" id="pDhw" d="M494 90H598"/>
<path class="pipe" id="pCoil" style="stroke-width:4.5" d="M598 90l40 10-36 14 36 14-36 14 36 14-36 14 36 14-36 14 36 14-36 14-12 4"/>
<path class="pipe" id="pDhwR" d="M590 232H555V525"/></g>
<path d="M547 400H563" style="stroke:var(--ea-card);stroke-width:14"/>
<g id="gSp" class="br act"><path class="pipe" id="pSpace" d="M482 102V400H600V485"/><path class="pipe" id="pLoop" style="stroke-width:4.5" d="M600 485H770V498H612V511H770V525H580"/></g>
<g class="valve"><path id="vL" d="M470 81V99L482 90Z"/><path id="vR" class="shut" d="M494 81V99L482 90Z"/><path id="vB" d="M473 102H491L482 90Z"/></g>
<text class="tl" x="494" y="128" data-t="3-way valve"></text><text class="ts" x="494" y="145" id="sV3"></text>
<circle class="eq" cx="440" cy="525" r="16"/><path d="M425 525l21-12v24z" style="fill:var(--ea-ink2)"/>
<text class="tl" x="440" y="556" text-anchor="middle" id="sPump"></text>
<g class="click" data-key="bar"><path class="ln" d="M500 506V525"/><circle class="eq" cx="500" cy="492" r="14"/><path id="needle" class="ln" style="stroke:var(--ea-hot);stroke-width:2.2" d="M500 492L491 499"/>
<text class="tv" x="500" y="468" text-anchor="middle" id="sBar"></text></g>
<rect x="346" y="519" width="14" height="12" rx="3" style="fill:var(--ea-ink2)"/>
<circle class="sens" cx="330" cy="190" r="4.5"/><circle class="sens" cx="390" cy="90" r="4.5"/><circle class="sens" cx="300" cy="440" r="4.5"/>
<g id="tags"></g>
<text class="ts" x="456" y="62" id="sLwSp"></text>
<g class="click" data-key="tank"><text class="tl" x="665" y="104" text-anchor="middle" data-t="Hot water"></text>
<text class="tb" x="665" y="148" text-anchor="middle" id="sTank"></text>
<text class="ts" x="665" y="172" text-anchor="middle" id="sDhwSp"></text></g>
<g class="click" data-key="room"><text class="tl" x="685" y="390" text-anchor="middle" data-t="Living room"></text>
<text class="tb" x="685" y="432" text-anchor="middle" id="sRoom"></text>
<text class="ts" x="685" y="456" text-anchor="middle" id="sRtSp"></text></g>
<rect x="24" y="488" width="136" height="7" rx="3.5" fill="url(#lg)"/>
<text class="ts" x="24" y="512">15°</text><text class="ts" x="92" y="512" text-anchor="middle">35°</text><text class="ts" x="160" y="512" text-anchor="end">55°</text>
<text class="tl" x="24" y="478" data-t="Water temperature"></text>
</svg>`;

class EspalthermaCard extends HTMLElement {
  setConfig(config) {
    this._config = config || {};
  }

  set hass(hass) {
    this._hass = hass;
    if (!this._root) this._build();
    this._paint();
  }

  _build() {
    this._root = this.attachShadow({mode: "open"});
    this._root.innerHTML = `<style>${STYLE}</style><ha-card${this._config.title ? ` header="${this._config.title}"` : ""}><div class="wrap">${SVG}</div></ha-card>`;
    const q = s => this._root.querySelector(s);
    q("#tags").innerHTML = TAGS.map(g => `<g class="tag" data-key="${g[1]}" transform="translate(${g[2]},${g[3]})"><rect width="${g[4]}" height="46" rx="14"/><text class="tl" x="12" y="18" style="font-size:11.5px" data-t="${g[0]}"></text><text class="tv" x="12" y="38" id="g_${g[1]}"></text></g>`).join("");
    this._root.querySelectorAll(".pipe").forEach(p => { const c = p.cloneNode(); c.removeAttribute("id"); c.setAttribute("class", "fl"); p.after(c); });
    // A tap on a reading opens its history
    this._root.addEventListener("click", ev => {
      const g = ev.composedPath().find(n => n.dataset && n.dataset.key);
      if (!g) return;
      const k = g.dataset.key;
      const id = (k === "room" && this._climate) ? this._climate : this._ent && this._ent[k];
      if (id) this.dispatchEvent(new CustomEvent("hass-more-info", {bubbles: true, composed: true, detail: {entityId: id}}));
    });
  }

  _paint() {
    const hass = this._hass, root = this._root, q = s => root.querySelector(s);
    const ent = this._ent = findEntities(hass, this._config);
    this._climate = this._config.climate || defaultClimate(hass);
    const dark = !!(hass.themes && hass.themes.darkMode);
    this.toggleAttribute("dark", dark);
    root.querySelectorAll("#lg stop").forEach((s, i) => s.setAttribute("stop-color", SCALE[dark ? "dark" : "light"][i]));
    root.querySelectorAll("[data-t]").forEach(n => { n.textContent = tr(hass, n.dataset.t); });
    const st = k => ent[k] && hass.states[ent[k]];
    const n = k => { const s = st(k); const v = s ? parseFloat(s.state) : NaN; return isNaN(v) ? null : v; };
    const on = k => { const s = st(k); return s ? s.state === "on" : null; };
    const f1 = v => (Math.round(v * 10) / 10).toFixed(1);
    const deg = v => (v == null ? "—" : f1(v) + "°");
    const tx = (id, s) => { q("#" + id).textContent = s; };
    const clim = this._climate && hass.states[this._climate];
    for (const [, k] of TAGS) { const v = n(k); tx("g_" + k, k === "flow" ? (v == null ? "—" : f1(Math.max(0, v)) + " l/min") : deg(v)); }
    const lw = n("lw2") != null ? n("lw2") : n("lw1"), ret = n("inlet"), tank = n("tank");
    const mid = (a, b) => (a != null && b != null ? (a + b) / 2 : a);
    [["pSup", lw], ["pDhw", lw], ["pCoil", mid(lw, tank)], ["pDhwR", ret], ["pSpace", lw], ["pLoop", mid(lw, ret)], ["pRet", ret]]
      .forEach(([id, v]) => { q("#" + id).style.stroke = tcol(v, dark); });
    q("#tankFill").style.fill = tcol(tank, dark);
    const v3 = on("v3"), flow = n("flow"), pump = on("pump") || (flow || 0) > 0.5, inv = n("inv");
    const svg = q("svg");
    q("#gDhw").classList.toggle("act", !!v3); q("#gSp").classList.toggle("act", !v3);
    q("#vR").classList.toggle("shut", !v3); q("#vB").classList.toggle("shut", !!v3);
    svg.classList.toggle("flowing", !!pump); svg.classList.toggle("comp", inv > 0); svg.classList.toggle("fanon", inv > 0);
    tx("sInv", inv == null ? "" : inv > 0 ? inv + " rps" : tr(hass, "Resting"));
    // measured once displayed: a text not on screen yet has no width
    requestAnimationFrame(() => { fit(q("#lComp"), 70); fit(q("#sInv"), 70); });
    tx("sV3", v3 == null ? "" : v3 ? tr(hass, "to the hot water") : tr(hass, "to the house"));
    tx("sPump", on("pump") == null && flow == null ? "" : pump ? tr(hass, "Pump on") : tr(hass, "Pump off"));
    const bar = n("bar");
    tx("sBar", bar == null ? "" : f1(bar) + " bar");
    const a = (-135 + (bar || 0) / 4 * 270) * Math.PI / 180;
    q("#needle").setAttribute("d", `M500 492L${(500 + 11 * Math.sin(a)).toFixed(1)} ${(492 - 11 * Math.cos(a)).toFixed(1)}`);
    tx("sTank", deg(tank));
    const room = clim && clim.attributes.current_temperature != null ? parseFloat(clim.attributes.current_temperature) : n("room");
    const target = clim && clim.state !== "off" && clim.attributes.temperature != null ? parseFloat(clim.attributes.temperature) : n("rtSp");
    tx("sRoom", deg(room));
    const target_ = v => tr(hass, "target {n}°", {n: Math.round(v * 10) / 10});
    tx("sRtSp", target != null ? target_(target) : clim && clim.state === "off" ? (hass.formatEntityState ? hass.formatEntityState(clim) : "off") : "");
    tx("sDhwSp", n("dhwSp") != null ? target_(n("dhwSp")) : "");
    tx("sLwSp", n("lwSp") != null ? target_(n("lwSp")) : "");
  }

  getCardSize() { return 7; }
  getGridOptions() { return {columns: 12, min_columns: 6}; }
  static getStubConfig() { return {}; }
}

// Dashboard strategy: the drawing, the thermostat, the key values and their history
class EspalthermaStrategy {
  static async generate(config, hass) {
    const ent = findEntities(hass, config);
    const climate = (config && config.climate) || defaultClimate(hass);
    const has = k => ent[k] && hass.states[ent[k]];
    const tiles = ["out", "tank", "inv", "flow", "bar", "mode", "err"].filter(has)
      .map(k => ({type: "tile", entity: ent[k], grid_options: {columns: 6}}));
    const water = ["lw2", "lw1", "inlet", "tank", "out"].filter(has).map(k => ent[k]);
    const sections = [
      {type: "grid", column_span: 3, cards: [{type: "custom:espaltherma-card", climate, grid_options: {columns: "full"}}]},
      {type: "grid", cards: [
        ...(climate ? [{type: "thermostat", entity: climate, grid_options: {columns: "full"}}] : []),
        ...tiles,
      ]},
    ];
    const history = [];
    if (water.length) history.push({type: "history-graph", title: tr(hass, "Water temperature"), hours_to_show: 24, entities: water, grid_options: {columns: "full"}});
    if (has("inv")) history.push({type: "history-graph", title: tr(hass, "Compressor"), hours_to_show: 24, entities: [ent.inv, ...(has("amp") ? [ent.amp] : [])], grid_options: {columns: "full"}});
    if (history.length) sections.push({type: "grid", column_span: 2, cards: history});
    return {title: "ESPAltherma", views: [{title: "ESPAltherma", path: "espaltherma", icon: "mdi:heat-pump", type: "sections", max_columns: 4, sections}]};
  }
}

if (!customElements.get("espaltherma-card")) customElements.define("espaltherma-card", EspalthermaCard);
if (!customElements.get("ll-strategy-dashboard-espaltherma")) customElements.define("ll-strategy-dashboard-espaltherma", class extends HTMLElement {
  static async generate(config, hass) { return EspalthermaStrategy.generate(config, hass); }
});
window.customCards = window.customCards || [];
if (!window.customCards.some(c => c.type === "espaltherma-card"))
  window.customCards.push({type: "espaltherma-card", name: "ESPAltherma", description: "Live drawing of the heat pump installation read by ESPAltherma", preview: false});
console.info("%c ESPALTHERMA-CARD %c " + VERSION + " ", "color:#fff;background:#0466c8;font-weight:700", "color:#0466c8");
