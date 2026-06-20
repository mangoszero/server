-- Anti-Cheat / Movement-Validation framework
-- Character database: persisted detection events.
-- Consumed by AntiCheatMgr::RecordViolation (insert) and `.anticheat report` (select).
-- Apply to the characters database (e.g. character0).

CREATE TABLE IF NOT EXISTS `character_anticheat_violation` (
  `id`      INT UNSIGNED      NOT NULL AUTO_INCREMENT,
  `guid`    INT UNSIGNED      NOT NULL                  COMMENT 'character low-guid',
  `account` INT UNSIGNED      NOT NULL DEFAULT 0,
  `time`    TIMESTAMP         NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `type`    TINYINT UNSIGNED  NOT NULL                  COMMENT 'AntiCheatViolationType',
  `score`   SMALLINT UNSIGNED NOT NULL DEFAULT 0        COMMENT 'decayed score at event time',
  `map`     SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  `x`       FLOAT             NOT NULL DEFAULT 0,
  `y`       FLOAT             NOT NULL DEFAULT 0,
  `z`       FLOAT             NOT NULL DEFAULT 0,
  `speed`   FLOAT             NOT NULL DEFAULT 0,
  `latency` SMALLINT UNSIGNED NOT NULL DEFAULT 0,
  `detail`  VARCHAR(128)      NOT NULL DEFAULT '',
  PRIMARY KEY (`id`),
  KEY `idx_guid_time` (`guid`, `time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COMMENT='Anti-cheat detection events';
