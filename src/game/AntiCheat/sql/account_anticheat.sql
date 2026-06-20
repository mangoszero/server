-- Anti-Cheat anti-gaming autoban
-- Realm (logon) database: per-account aggregate for the autoban accumulator.
-- Decays slowly (hours) so spacing offences out still accumulates toward a ban.
-- Apply to the realmd database.

CREATE TABLE IF NOT EXISTS `account_anticheat` (
  `account`     INT UNSIGNED NOT NULL          COMMENT 'account id (realmd.account.id)',
  `kick_score`  FLOAT        NOT NULL DEFAULT 0 COMMENT 'decaying accumulated kick weight',
  `ban_count`   INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'how many autobans issued (escalation tier)',
  `last_update` INT UNSIGNED NOT NULL DEFAULT 0 COMMENT 'unix seconds of last update (for decay)',
  PRIMARY KEY (`account`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COMMENT='Anti-cheat per-account autoban accumulator';
