-- The roster is intentionally in the characters database.  It contains only
-- persistent custom Altbots; it does not participate in playerbots random-account tables.
CREATE TABLE IF NOT EXISTS `custom_playerbots` (
  `guid` INT UNSIGNED NOT NULL,
  `account_id` INT UNSIGNED NOT NULL,
  `autologin` TINYINT UNSIGNED NOT NULL DEFAULT 1,
  `autonomous` TINYINT UNSIGNED NOT NULL DEFAULT 1,
  `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`guid`),
  KEY `idx_custom_playerbots_autologin` (`autologin`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
