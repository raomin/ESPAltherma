#ifndef ESPALTHERMA_DEFROSTWEB_H
#define ESPALTHERMA_DEFROSTWEB_H
#include "defrostexport.h"
#include <atomic>

// Included after the web helpers. Only explicit user actions download/annotate/report a case.
static std::atomic<bool> defrostReportPending{false}; // web handler -> main-loop handoff
static bool defrostReportSending=false; // protected by valuesLock
static uint32_t defrostReportId=0;
static int defrostReportCode=0;

static void defrostWebStatus(JsonObject out)
{
  defrostStatusJson(out);
  out["report_available"]=TELEMETRY_AVAILABLE;
  valuesLock();
  bool same=out["active"].as<bool>() && out["id"].as<uint32_t>()==defrostReportId;
  const char *state="idle";
  if (same)
  {
    if (out["reported"].as<bool>()) state="sent";
    else if (defrostReportPending) state="queued";
    else if (defrostReportSending) state="sending";
    else if (defrostReportCode && (defrostReportCode<200 || defrostReportCode>=300)) state="failed";
  }
  out["report_state"]=state;out["report_code"]=same?defrostReportCode:0;
  valuesUnlock();
}

static uint32_t defrostBootId()
{
  static uint32_t id=esp_random();
  return id;
}

// A transient, immutable snapshot per download. Formatting uses one row at a time, not a full trace JSON tree.
struct DefrostDownload
{
  EarlyDefrostEvent event;
  String pending;
  size_t cursor=0;
  uint16_t row=0;
  uint32_t seconds=0;
  bool csv=false, ending=false;

  bool begin(bool asCsv=false,bool forTelemetry=false)
  {
    csv=asCsv;
    if (csv)
    {
      if (!pending.reserve(768)) return false;
      pending="# Early defrost is a timing observation, not proof of an unnecessary defrost.\n";
      pending+="# Check the outdoor heat exchanger for ice BEFORE the next defrost.\n";
      pending+="# event_id,"+String(event.id)+"\n# detected_epoch_utc,"+String(event.epoch)+"\n";
      pending+="# seconds_after_first_observed_run,"+String(event.elapsedSeconds)+"\n";
      pending+="# start_uncertainty_seconds,"+String(event.startUncertaintySeconds)+"\n";
      pending+="# observation,"+String(iceObservationName(event.observation))+"\n";
      pending+="# temperatures_quantised_to_0.5_C; read times do not establish the outdoor sampling time\n";
      pending+="seconds_since_observed_start,outdoor_c,coil_c,rps,startup,defrost,dhw,cooling\n";
      return pending.endsWith("\n");
    }
    JsonDocument doc;
    doc["v"]=1;doc["fw"]=ESPALTHERMA_VERSION;doc["board"]=BOARD_NAME;doc["model"]=config.model;
    doc["boot_id"]=defrostBootId();
    if (forTelemetry) {doc["install_id"]=config.installId;doc["user_consent"]=true;}
    JsonObject e=doc["event"].to<JsonObject>();
    e["id"]=event.id;e["detected_epoch"]=event.epoch;e["observed_start_uptime_ms"]=event.trace.startMs;
    e["elapsed_s"]=event.elapsedSeconds;e["start_uncertainty_s"]=event.startUncertaintySeconds;
    if (event.outdoorTenths==INT16_MIN) e["outdoor_c"]=nullptr;else e["outdoor_c"]=event.outdoorTenths/10.0;
    if (event.coilTenths==INT16_MIN) e["coil_c"]=nullptr;else e["coil_c"]=event.coilTenths/10.0;
    e["observation"]=iceObservationName(event.observation);e["truncated"]=event.trace.truncated;
    e["recording_complete"]=(uint32_t)(millis()-event.trace.startMs)>DEFROST_WINDOW_MS;
    reportedIdentityJson(doc["identity"].to<JsonObject>());
    doc["sample_columns"]="delta_s,outdoor_half_c,coil_half_c,rps,flags";
    doc["temperature_scale_c"]=0.5;doc["temperature_unknown"]=-128;doc["rps_unknown"]=255;
    JsonObject flags=doc["flags"].to<JsonObject>();
    flags["startup"]=DF_STARTUP;flags["defrost"]=DF_DEFROST;flags["dhw"]=DF_DHW;
    flags["cooling"]=DF_COOLING;flags["status_known"]=DF_STATUS;flags["dhw_known"]=DF_DHW_KNOWN;
    doc["interpretation"]="Possible unnecessary defrost during startup, not proven by temperatures alone. Check whether the outdoor heat exchanger is ice-free BEFORE the next defrost. Times refer to local X10A reads, not confirmed outdoor sampling times.";
    size_t expected=measureJson(doc);
    size_t written=serializeJson(doc,pending);
    if (doc.overflowed() || written!=expected || pending.length()!=expected || !pending.endsWith("}")) return false;
    pending.remove(pending.length()-1);
    pending+=",\"samples\":[\n";
    return pending.endsWith("[\n");
  }

  bool next()
  {
    if (row>=event.trace.count)
    {
      if (ending) return false;
      ending=true;pending=csv?"":"\n]}\n";cursor=0;
      return pending.length()!=0;
    }
    const DefrostSample &s=event.trace.samples[row];
    char text[112];
    if (csv)
    {
      seconds+=s.deltaSeconds;
      defrostCsvRow(s,seconds,text,sizeof(text));
    }
    else
      defrostJsonRow(s,row==0,text,sizeof(text));
    row++;pending=text;cursor=0;
    return true;
  }
  size_t fill(uint8_t *out,size_t maxLen)
  {
    size_t used=0;
    while (used<maxLen)
    {
      if (cursor==pending.length() && !next()) break;
      size_t n=pending.length()-cursor;
      if (n>maxLen-used) n=maxLen-used;
      memcpy(out+used,pending.c_str()+cursor,n);used+=n;cursor+=n;
    }
    return used;
  }
};
static std::weak_ptr<DefrostDownload> activeDefrostDownload;

static bool defrostObservation(JsonDocument &doc,IceObservation &out)
{
  const char *value=doc["observation"]|"not_checked";
  if (!strcmp(value,"not_checked")) out=ICE_NOT_CHECKED;
  else if (!strcmp(value,"ice_free")) out=ICE_FREE;
  else if (!strcmp(value,"iced")) out=ICE_PRESENT;
  else return false;
  return true;
}

static void registerDefrostRoutes()
{
  webServer.on("/api/defrost/download",HTTP_GET,[](AsyncWebServerRequest *r) {
    if (!activeDefrostDownload.expired()) {r->send(409,"application/json","{\"ok\":false,\"error\":\"download already in progress\"}");return;}
    auto snapshot=std::shared_ptr<DefrostDownload>(new (std::nothrow) DefrostDownload());
    if (!snapshot) {r->send(503,"application/json","{\"ok\":false,\"error\":\"not enough memory\"}");return;}
    if (!defrostSnapshot(snapshot->event)) {r->send(404,"application/json","{\"ok\":false,\"error\":\"no current case\"}");return;}
    bool csv=r->hasParam("format") && r->getParam("format")->value()=="csv";
    if (!snapshot->begin(csv)) {r->send(503,"application/json","{\"ok\":false,\"error\":\"not enough memory\"}");return;}
    activeDefrostDownload=snapshot;
    AsyncWebServerResponse *response=r->beginChunkedResponse(csv?"text/csv":"application/json",
      [snapshot](uint8_t *out,size_t maxLen,size_t)->size_t {return snapshot->fill(out,maxLen);});
    response->addHeader("Content-Disposition",csv?"attachment; filename=espaltherma-early-defrost.csv":"attachment; filename=espaltherma-early-defrost.json");
    response->addHeader("Cache-Control","no-store");
    r->send(response);
  });
  webServer.on("/api/defrost/observation",HTTP_POST,[](AsyncWebServerRequest *r) {
    if (!authorized(r)) return;
    JsonDocument doc;if (!parseBody(r,doc)) return;
    IceObservation observation;
    if (!defrostObservation(doc,observation)) {r->send(400,"application/json","{\"ok\":false,\"error\":\"observation\"}");return;}
    if (!defrostAnnotate(doc["id"]|uint32_t(0),observation)) {r->send(409,"application/json","{\"ok\":false,\"error\":\"case expired or replaced\"}");return;}
    sendOk(r);
  },nullptr,collectBody);
  webServer.on("/api/defrost/report",HTTP_POST,[](AsyncWebServerRequest *r) {
    if (!authorized(r)) return;
    JsonDocument doc;if (!parseBody(r,doc)) return;
    IceObservation observation;
    if (!doc["user_consent"].is<bool>() || !doc["user_consent"].as<bool>() || !defrostObservation(doc,observation))
    {r->send(400,"application/json","{\"ok\":false,\"error\":\"explicit consent and valid observation required\"}");return;}
    if (!TELEMETRY_AVAILABLE) {r->send(503,"application/json","{\"ok\":false,\"error\":\"telemetry not configured\"}");return;}
    uint32_t id=doc["id"]|uint32_t(0);
    if (!defrostAnnotate(id,observation)) {r->send(409,"application/json","{\"ok\":false,\"error\":\"case expired or replaced\"}");return;}
    valuesLock();
    bool busy=defrostReportPending||defrostReportSending;
    if (!busy) {defrostReportId=id;defrostReportCode=0;defrostReportPending=true;}
    valuesUnlock();
    if (busy) {r->send(409,"application/json","{\"ok\":false,\"error\":\"report already in progress\"}");return;}
    sendOk(r);
  },nullptr,collectBody);
}

// Main loop only, once per explicit click. The general automatic-telemetry checkbox does not send these cases.
static void sendDefrostReport()
{
  valuesLock();
  bool pending=defrostReportPending;
  uint32_t id=defrostReportId;
  if (pending) {defrostReportPending=false;defrostReportSending=true;}
  valuesUnlock();
  if (!pending) return;
  LOOP_STAGE("early defrost report");
  int code=-2;
  auto snapshot=std::unique_ptr<DefrostDownload>(new (std::nothrow) DefrostDownload());
  if (snapshot && defrostSnapshot(snapshot->event) && snapshot->event.id==id)
  {
    bool rendered=snapshot->begin(false,true);
    String payload;
    if (rendered && payload.reserve(8192))
    {
      uint8_t chunk[256];size_t n;
      bool fits=true;
      while ((n=snapshot->fill(chunk,sizeof(chunk)))!=0)
      {
        if (payload.length()+n>8192) {fits=false;break;}
        if (!payload.concat(reinterpret_cast<const char *>(chunk),n)) {fits=false;break;}
      }
      if (fits && TELEMETRY_AVAILABLE && netOnline() && tlsHasRoom())
      {
        String url=TELEMETRY_URL;
        int slash=url.lastIndexOf('/');
        url=url.substring(0,slash+1)+"defrost";
        TlsWatch watch;WiFiClientSecure tls;tls.setInsecure();HTTPClient http;
        http.setConnectTimeout(5000);http.setTimeout(10000);
        if (http.begin(tls,url)) {http.addHeader("Content-Type","application/json");code=http.POST(payload);http.end();}
        else code=-1;
      }
      else if (!fits) code=413;
    }
    if (code>=200 && code<300) defrostMarkReported(id,snapshot->event.observation);
  }
  else code=409;
  valuesLock();defrostReportCode=code;defrostReportSending=false;valuesUnlock();
  mqttSerial.printf("Early defrost report %s (%d)\n",code>=200&&code<300?"sent":"not sent",code);
}

#endif
