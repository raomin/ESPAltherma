#pragma once

// Included after the web server's JSON helpers. Expert state is intentionally absent from config/NVS.
static bool expertAuthorized(AsyncWebServerRequest *r, bool writing = false)
{
  if (!config.adminPwd[0])
  {
    r->send(403, "application/json", "{\"error\":\"set an admin password first\"}");
    return false;
  }
  if (!authorized(r)) return false;
  if (writing && (!r->hasHeader("X-ESPAltherma-Expert") ||
                  r->getHeader("X-ESPAltherma-Expert")->value() != "1" ||
                  r->contentType() != "application/json"))
  {
    r->send(400, "application/json", "{\"error\":\"application/json and X-ESPAltherma-Expert: 1 required\"}");
    return false;
  }
  return true;
}

static String expertHexString(const uint8_t *bytes, size_t len)
{
  String hex;
  hex.reserve(len * 2);
  static const char digits[] = "0123456789ABCDEF";
  for (size_t i = 0; i < len; ++i)
  {
    hex += digits[bytes[i] >> 4];
    hex += digits[bytes[i] & 15];
  }
  return hex;
}

static void expertStatus(AsyncWebServerRequest *r)
{
  ExpertJob job;
  JsonDocument doc;
  expertLock();
  uint32_t now = millis();
  bool enabled = expertSession.active(now);
  bool pending = expertSession.pending, running = expertSession.running, done = expertSession.done;
  uint32_t remaining = enabled ? ExpertSession::LEASE_MS - uint32_t(now - expertSession.enabledAt) : 0;
  job = expertSession.job;
  expertUnlock();
  doc["enabled"] = enabled;
  doc["expires_in_ms"] = remaining;
  doc["pending"] = pending;
  doc["running"] = running;
  doc["done"] = done;
  doc["id"] = job.id;
  doc["tx"] = expertHexString(job.tx, job.txLen);
  doc["rx"] = expertHexString(job.rx, job.rxLen);
  doc["sent"] = job.sent;
  doc["received"] = job.rxLen;
  doc["started_ms"] = job.startedMs;
  doc["elapsed_ms"] = job.elapsedMs;
  doc["timeout"] = job.timedOut;
  doc["truncated"] = job.truncated;
  doc["cancelled"] = job.cancelled;
  sendJson(r, doc);
}

static void expertSetupRoutes()
{
  // Longer route first: AsyncWebServer matches path prefixes.
  webServer.on("/api/expert/x10a", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!expertAuthorized(r, true)) return;
    JsonDocument doc;
    if (!parseBody(r, doc)) return;
    ExpertJob job;
    if (!doc["frame"].is<const char *>() ||
        !expertHex(doc["frame"].as<const char *>(), job.tx, sizeof(job.tx), job.txLen) ||
        (!doc["timeout_ms"].isNull() && !doc["timeout_ms"].is<int>()) ||
        (!doc["idle_ms"].isNull() && !doc["idle_ms"].is<int>()))
    {
      r->send(400, "application/json", "{\"error\":\"frame must be 1..256 hex bytes; timing must be integer milliseconds\"}");
      return;
    }
    int timeout = doc["timeout_ms"] | 500, idle = doc["idle_ms"] | 100;
    if (timeout < 20 || timeout > 2000 || idle < 5 || idle > timeout)
    {
      r->send(400, "application/json", "{\"error\":\"timeout_ms: 20..2000; idle_ms: 5..timeout_ms\"}");
      return;
    }
    job.timeoutMs = timeout;
    job.idleMs = idle;
    expertLock();
    bool enabled = expertSession.active(millis());
    bool accepted = enabled && expertSession.submit(job, millis());
    uint32_t id = expertSession.job.id;
    expertUnlock();
    if (!accepted)
    {
      r->send(enabled ? 409 : 403, "application/json", enabled ? "{\"error\":\"transaction busy\"}" : "{\"error\":\"expert mode disabled\"}");
      return;
    }
    doc.clear();
    doc["id"] = id;
    doc["accepted"] = true;
    sendJson(r, doc, 202);
  }, nullptr, collectBody);
  webServer.on("/api/expert/x10a", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (expertAuthorized(r)) expertStatus(r);
  });
  webServer.on("/api/expert", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (!expertAuthorized(r, true)) return;
    JsonDocument doc;
    if (!parseBody(r, doc)) return;
    if (!doc["enabled"].is<bool>())
    {
      r->send(400, "application/json", "{\"error\":\"enabled must be a boolean\"}");
      return;
    }
    bool enabled = doc["enabled"].as<bool>();
    expertLock();
    expertSession.setEnabled(enabled, millis());
    expertUnlock();
    eventAddf("X10A expert mode %s from %s", enabled ? "enabled for 15 min" : "disabled", r->client()->remoteIP().toString().c_str());
    expertStatus(r);
  }, nullptr, collectBody);
  webServer.on("/api/expert", HTTP_GET, [](AsyncWebServerRequest *r) {
    if (expertAuthorized(r)) expertStatus(r);
  });
}
