-- The updater sorts by filename, not by base/updates directory. Bootstrap here
-- because this dated migration sorts before base/custom_playerbots.sql.
-- Keep the fresh-install schema in sync with that base file.
CREATE TABLE IF NOT EXISTS `custom_playerbots` (
  `guid` INT UNSIGNED NOT NULL,
  `account_id` INT UNSIGNED NOT NULL,
  `autologin` TINYINT UNSIGNED NOT NULL DEFAULT 1,
  `autonomous` TINYINT UNSIGNED NOT NULL DEFAULT 1,
  `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`guid`),
  KEY `idx_custom_playerbots_autologin` (`autologin`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- Existing pre-autonomous rosters retain the original migration's opt-in
-- behavior (0). Already migrated rosters keep their values and column default.
-- Use a metadata guard rather than MariaDB-only ADD COLUMN IF NOT EXISTS.
SET @custom_playerbots_autonomous_sql = IF(
  EXISTS (
    SELECT 1 FROM `information_schema`.`COLUMNS`
    WHERE `TABLE_SCHEMA` = DATABASE()
      AND `TABLE_NAME` = 'custom_playerbots'
      AND `COLUMN_NAME` = 'autonomous'
  ),
  'SELECT 1',
  'ALTER TABLE `custom_playerbots` ADD COLUMN `autonomous` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `autologin`'
);
PREPARE custom_playerbots_autonomous_stmt FROM @custom_playerbots_autonomous_sql;
EXECUTE custom_playerbots_autonomous_stmt;
DEALLOCATE PREPARE custom_playerbots_autonomous_stmt;
SET @custom_playerbots_autonomous_sql = NULL;
