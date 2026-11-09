-- Heat pump reports (functions/api/report.js). One row per board, identification key and model.
-- Databases created before the location columns: ALTER TABLE reports ADD COLUMN country TEXT; ... ADD COLUMN city TEXT;
CREATE TABLE IF NOT EXISTS reports (
  install_id TEXT NOT NULL,     -- random id of the board, 16 hex digits
  key TEXT NOT NULL,            -- identification key of the heat pump (survey)
  model TEXT NOT NULL,          -- definition chosen or confirmed by the owner
  confirmed INTEGER NOT NULL,   -- 1: confirmed by the owner
  fw TEXT,                      -- ESPAltherma version
  board TEXT,                   -- ESP32 board
  protocol TEXT,                -- I or S
  fixes TEXT,                   -- registries corrected on this unit (JSON)
  report TEXT NOT NULL,         -- the survey report (JSON)
  country TEXT,                 -- where the report came from, as Cloudflare locates the sender (the IP is not stored)
  city TEXT,
  first_seen TEXT NOT NULL,     -- day of the first report
  last_seen TEXT NOT NULL,      -- day of the last report
  count INTEGER NOT NULL DEFAULT 1,
  PRIMARY KEY (install_id, key, model)
);
CREATE INDEX IF NOT EXISTS reports_key ON reports (key);
CREATE INDEX IF NOT EXISTS reports_day ON reports (last_seen);

-- Daily heartbeats (functions/api/heartbeat.js), from 2.0.0-beta.27: one row per board and day, the last one of the
-- day kept. Tells which boards are alive, on which firmware, and which optional features they run; no readings.
CREATE TABLE IF NOT EXISTS heartbeats (
  install_id TEXT NOT NULL,     -- the board's random id, the same as in reports
  day TEXT NOT NULL,            -- the day (UTC)
  fw TEXT,                      -- ESPAltherma version
  board TEXT,                   -- ESP32 board
  model TEXT,                   -- definition in use
  protocol TEXT,                -- I or S
  sw_id TEXT,                   -- hydro board software, when known
  unit_model TEXT,              -- the unit's name plate model, when the board holds it
  uptime INTEGER,               -- seconds since the board's start
  features TEXT,                -- the optional features' status (JSON: dhw_target, lwt_target, power_limit, meter, diii, stats)
  country TEXT,                 -- as Cloudflare locates the sender (the IP is not stored)
  PRIMARY KEY (install_id, day)
);
CREATE INDEX IF NOT EXISTS heartbeats_day ON heartbeats (day);
