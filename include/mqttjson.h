#ifndef ESPALTHERMA_MQTTJSON_H
#define ESPALTHERMA_MQTTJSON_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Keep the order of the poll, and omit labels whose registry did not answer in this cycle.
template <typename Label, typename Visit>
void mqttVisitSelected(const Label *labels, size_t count, const uint8_t *registries, size_t registryCount, Visit visit)
{
  for (size_t r = 0; r < registryCount; r++)
    for (size_t i = 0; i < count; i++)
      if ((uint8_t)labels[i].registryID == registries[r])
        visit(labels[i]);
}

// One pass counts bytes, a second writes the same fields in small chunks. The caller keeps the
// readings locked and snapshots clocks/diagnostics before either pass. No complete payload is stored.
class MqttJson
{
public:
  typedef bool (*Sink)(void *, const char *, size_t);
  static const size_t CHUNK_SIZE = 256;

  explicit MqttJson(Sink sink = nullptr, void *ctx = nullptr) : sink(sink), ctx(ctx) {}

  void begin(bool table) { raw(table ? "[{" : "{"); }
  void end(bool table) { raw(table ? "}]" : "}"); }
  size_t size() const { return total; }
  bool good() const { return ok; }

  bool flush()
  {
    if (ok && sink && used)
      ok = sink(ctx, chunk, used);
    used = 0;
    return ok;
  }

  void text(const char *key, const char *value)
  {
    field(key);
    quoted(value);
  }

  void number(const char *key, const char *value)
  {
    field(key);
    raw(value); // a formatted number, or null
  }

  template <typename... Args>
  void format(const char *key, const char *fmt, Args... args)
  {
    char value[48];
    int n = snprintf(value, sizeof(value), fmt, args...);
    if (n < 0 || (size_t)n >= sizeof(value))
    {
      ok = false;
      return;
    }
    number(key, value);
  }

  // The same numeric/string choice as the previous ATTR builder, including '-' as text.
  void converted(const char *key, const char *value, bool binaryAsNumber)
  {
    value = binaryValue(value, binaryAsNumber);
    bool alpha = false, digit = false;
    for (size_t i = 0; value[i]; i++)
    {
      char c = value[i];
      if (c >= '0' && c <= '9')
        digit = true;
      else if (c != '.' && !(c == '-' && i == 0))
        alpha = true;
    }
    if (alpha || !digit)
      text(key, value);
    else
      number(key, value);
  }

  static const char *binaryValue(const char *value, bool binaryAsNumber)
  {
    if (binaryAsNumber && strcmp(value, "ON") == 0)
      return "1";
    if (binaryAsNumber && strcmp(value, "OFF") == 0)
      return "0";
    return value;
  }

private:
  Sink sink;
  void *ctx;
  char chunk[CHUNK_SIZE];
  size_t used = 0, total = 0;
  bool first = true, ok = true;

  void raw(const char *s)
  {
    size_t n = strlen(s);
    total += n;
    if (!sink || !ok)
      return;
    while (n && ok)
    {
      size_t take = n < CHUNK_SIZE - used ? n : CHUNK_SIZE - used;
      memcpy(chunk + used, s, take);
      used += take;
      s += take;
      n -= take;
      if (used == CHUNK_SIZE)
        flush();
    }
  }

  void quoted(const char *s)
  {
    raw("\"");
    for (; *s; s++)
    {
      unsigned char c = (unsigned char)*s;
      if (c == '"' || c == '\\')
        raw("\\");
      if (c < 0x20)
      {
        char escaped[7];
        snprintf(escaped, sizeof(escaped), "\\u%04x", (unsigned)c);
        raw(escaped);
      }
      else
      {
        char one[2] = {*s, 0};
        raw(one);
      }
    }
    raw("\"");
  }

  void field(const char *key)
  {
    if (!first)
      raw(",");
    first = false;
    quoted(key);
    raw(":");
  }
};

// PubSubClient's streaming header still uses a uint16_t remaining length. Reject an oversized
// packet before beginning it; a failed/short write must make the caller close the TCP connection.
template <typename Client, typename Render>
bool mqttStreamJson(Client &client, const char *topic, bool retained, Render render)
{
  MqttJson count;
  render(count);
  size_t topicLen = strlen(topic);
  if (!count.good() || topicLen + 7 > client.getBufferSize() || topicLen + 2 > UINT16_MAX ||
      count.size() > UINT16_MAX - topicLen - 2 || !client.connected())
    return false;
  if (!client.beginPublish(topic, (unsigned int)count.size(), retained))
    return false;

  MqttJson output([](void *ctx, const char *data, size_t len) {
    return static_cast<Client *>(ctx)->write(reinterpret_cast<const uint8_t *>(data), len) == len;
  }, &client);
  render(output);
  return output.flush() && output.size() == count.size() && client.endPublish();
}

#endif
