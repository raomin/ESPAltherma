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
