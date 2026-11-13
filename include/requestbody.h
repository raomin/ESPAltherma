#ifndef ESPALTHERMA_REQUESTBODY_H
#define ESPALTHERMA_REQUESTBODY_H

#define MAX_BODY_SIZE 8192

// ESPAsyncWebServer owns/frees _tempObject with the request. Included after the web/JSON types.
static void collectBody(AsyncWebServerRequest *r, uint8_t *data, size_t len, size_t index, size_t total)
{
  if (total > MAX_BODY_SIZE || index > total || len > total-index)
    return;
  if (index == 0)
    r->_tempObject = calloc(total + 1, 1);
  if (r->_tempObject != nullptr)
    memcpy((uint8_t *)r->_tempObject + index, data, len);
}

static bool parseBody(AsyncWebServerRequest *r, JsonDocument &doc)
{
  if (r->contentLength() > MAX_BODY_SIZE)
  {
    r->send(413, "application/json", "{\"ok\":false,\"error\":\"request body too large\"}");
    return false;
  }
  if (r->_tempObject == nullptr || deserializeJson(doc, (const char *)r->_tempObject) != DeserializationError::Ok)
  {
    r->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid JSON\"}");
    return false;
  }
  return true;
}

#endif
