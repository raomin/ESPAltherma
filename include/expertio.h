#pragma once
#include "expert.h"

static ExpertSession expertSession;
static SemaphoreHandle_t expertMutex = xSemaphoreCreateMutex();
static void expertLock() { xSemaphoreTake(expertMutex, portMAX_DELAY); }
static void expertUnlock() { xSemaphoreGive(expertMutex); }

// Heat pump task only. The serial mutex excludes polling, controls and the MQTT gateway.
// Unlike x10aExchange, this preserves echoes, NAKs, invalid CRCs and partial responses.
static void expertTick()
{
  ExpertJob job;
  expertLock();
  bool take = expertSession.take(job, millis());
  expertUnlock();
  if (!take) return;

  serialLock();
  MySerial.flush(SERIAL_FLUSH_TX_ONLY);
  // Discard old bytes, with a bound even if the peer keeps sending.
  for (size_t i = 0; i < 1024 && MySerial.available(); ++i) MySerial.read();
  job.startedMs = millis();
  job.sent = MySerial.write(job.tx, job.txLen);
  uint32_t last = job.startedMs;
  bool received = false;
  while (uint32_t(millis() - job.startedMs) < job.timeoutMs)
  {
    if (MySerial.available())
    {
      int b = MySerial.read();
      if (job.rxLen < ExpertJob::RX_MAX) job.rx[job.rxLen++] = (uint8_t)b;
      else job.truncated = true;
      last = millis();
      received = true;
    }
    else
    {
      if (received && uint32_t(millis() - last) >= job.idleMs) break;
      delay(1);
    }
  }
  job.elapsedMs = millis() - job.startedMs;
  job.timedOut = job.elapsedMs >= job.timeoutMs;
  serialUnlock();

  expertLock();
  expertSession.finish(job);
  expertUnlock();
}
