-- Run once against the characters database when updating an existing installation.
ALTER TABLE `custom_playerbots`
  ADD COLUMN `autonomous` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `autologin`;
