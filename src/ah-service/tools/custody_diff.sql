-- MaNGOS Zero AH custody player-visible differential.
--
-- Preconditions:
--   * Run one identical, seeded workload on two quiesced Character DB clones:
--       A: AH.Service.Custody = 0
--       B: AH.Service.Custody = 1
--   * Use named test characters, items, and auction ids; keep AH bot off.
--   * Start both clones from the same idle MAX(mail.id) and MAX(auction.id)
--     snapshot. This script compares mail_items.mail_id and auction.id
--     directly, as required by the player-visible projection.
--   * Allow no other logins or background auction/mail activity while replaying.
--   * Prefer a pinned/normalized clock. If wall-clock timestamps differ between
--     clone runs, keep @compare_absolute_mail_times = 0 and
--     @compare_absolute_auction_times = 0 so the mail deliver/expire times and
--     the auction.time (expiry) column are dumped for inspection but excluded
--     from the diff. Custody never alters these columns; only clock drift does.
--   * custody_ledger is intentionally excluded: only player-visible tables are
--     compared here.
--
-- The Character DB schema has no persisted "sent" or "created_time" column.
-- SendMailToInTransaction computes deliver_time as time(NULL) + deliver_delay,
-- and custody AH mail uses deliver_delay = 0. This script reports the stored
-- deliver/expire windows and can either compare absolute mail times or normalize
-- them out; proving "deliver_time - sent == 0" requires the workload harness or
-- source review because "sent" is not stored.
--
-- Usage:
--   1. Edit the schema names below, or set @clone_a/@clone_b before sourcing.
--   2. mysql --table -u <user> -p < src/ah-service/tools/custody_diff.sql
--   3. Every *_DIFF section must return zero rows.

SET @clone_a := COALESCE(@clone_a, 'character0_custody_off');
SET @clone_b := COALESCE(@clone_b, 'character0_custody_on');
SET @compare_absolute_mail_times := COALESCE(@compare_absolute_mail_times, 0);
SET @compare_absolute_auction_times := COALESCE(@compare_absolute_auction_times, 0);

SET @a := CONCAT('`', REPLACE(@clone_a, '`', '``'), '`');
SET @b := CONCAT('`', REPLACE(@clone_b, '`', '``'), '`');

SELECT 'custody_diff inputs' AS section,
       @clone_a AS clone_a_custody_off,
       @clone_b AS clone_b_custody_on,
       @compare_absolute_mail_times AS compare_absolute_mail_times;

SELECT 'RAW_A_CHARACTERS: SELECT guid, money FROM characters ORDER BY guid' AS section;
SET @sql := CONCAT('SELECT guid, money FROM ', @a, '.`characters` ORDER BY guid');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SELECT 'RAW_B_CHARACTERS: SELECT guid, money FROM characters ORDER BY guid' AS section;
SET @sql := CONCAT('SELECT guid, money FROM ', @b, '.`characters` ORDER BY guid');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SELECT 'RAW_A_MAIL: full spec columns incl sender/messageType/stationery/cod/expire_time' AS section;
SET @sql := CONCAT(
    'SELECT receiver, sender, messageType, stationery, money, cod, subject, body, has_items, checked, deliver_time, expire_time ',
    'FROM ', @a, '.`mail` ORDER BY receiver, deliver_time, money, subject');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SELECT 'RAW_B_MAIL: full spec columns incl sender/messageType/stationery/cod/expire_time' AS section;
SET @sql := CONCAT(
    'SELECT receiver, sender, messageType, stationery, money, cod, subject, body, has_items, checked, deliver_time, expire_time ',
    'FROM ', @b, '.`mail` ORDER BY receiver, deliver_time, money, subject');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SELECT 'RAW_A_MAIL_ITEMS: SELECT mail_id, item_guid, item_template, receiver FROM mail_items ORDER BY receiver, item_guid' AS section;
SET @sql := CONCAT('SELECT mail_id, item_guid, item_template, receiver FROM ', @a, '.`mail_items` ORDER BY receiver, item_guid');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SELECT 'RAW_B_MAIL_ITEMS: SELECT mail_id, item_guid, item_template, receiver FROM mail_items ORDER BY receiver, item_guid' AS section;
SET @sql := CONCAT('SELECT mail_id, item_guid, item_template, receiver FROM ', @b, '.`mail_items` ORDER BY receiver, item_guid');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SELECT 'RAW_A_ITEM_INSTANCE: SELECT guid, owner_guid FROM item_instance ORDER BY guid' AS section;
SET @sql := CONCAT('SELECT guid, owner_guid FROM ', @a, '.`item_instance` ORDER BY guid');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SELECT 'RAW_B_ITEM_INSTANCE: SELECT guid, owner_guid FROM item_instance ORDER BY guid' AS section;
SET @sql := CONCAT('SELECT guid, owner_guid FROM ', @b, '.`item_instance` ORDER BY guid');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SELECT 'RAW_A_AUCTION: SELECT * FROM auction ORDER BY id' AS section;
SET @sql := CONCAT('SELECT * FROM ', @a, '.`auction` ORDER BY id');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SELECT 'RAW_B_AUCTION: SELECT * FROM auction ORDER BY id' AS section;
SET @sql := CONCAT('SELECT * FROM ', @b, '.`auction` ORDER BY id');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

DROP TEMPORARY TABLE IF EXISTS custody_diff_characters_a;
DROP TEMPORARY TABLE IF EXISTS custody_diff_characters_b;
DROP TEMPORARY TABLE IF EXISTS custody_diff_mail_a;
DROP TEMPORARY TABLE IF EXISTS custody_diff_mail_b;
DROP TEMPORARY TABLE IF EXISTS custody_diff_mail_items_a;
DROP TEMPORARY TABLE IF EXISTS custody_diff_mail_items_b;
DROP TEMPORARY TABLE IF EXISTS custody_diff_item_instance_a;
DROP TEMPORARY TABLE IF EXISTS custody_diff_item_instance_b;
DROP TEMPORARY TABLE IF EXISTS custody_diff_auction_a;
DROP TEMPORARY TABLE IF EXISTS custody_diff_auction_b;

SET @sql := CONCAT('CREATE TEMPORARY TABLE custody_diff_characters_a AS ',
                   'SELECT guid, money FROM ', @a, '.`characters`');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;
SET @sql := CONCAT('CREATE TEMPORARY TABLE custody_diff_characters_b AS ',
                   'SELECT guid, money FROM ', @b, '.`characters`');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @mail_time_cols := IF(@compare_absolute_mail_times = 1,
                          'deliver_time, expire_time, CAST(NULL AS SIGNED) AS expire_delay',
                          'CAST(NULL AS UNSIGNED) AS deliver_time, CAST(NULL AS UNSIGNED) AS expire_time, CAST(expire_time AS SIGNED) - CAST(deliver_time AS SIGNED) AS expire_delay');
SET @sql := CONCAT('CREATE TEMPORARY TABLE custody_diff_mail_a AS ',
                   'SELECT receiver, sender, messageType, stationery, money, cod, subject, body, has_items, checked, ',
                   @mail_time_cols, ' FROM ', @a, '.`mail`');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;
SET @sql := CONCAT('CREATE TEMPORARY TABLE custody_diff_mail_b AS ',
                   'SELECT receiver, sender, messageType, stationery, money, cod, subject, body, has_items, checked, ',
                   @mail_time_cols, ' FROM ', @b, '.`mail`');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @sql := CONCAT('CREATE TEMPORARY TABLE custody_diff_mail_items_a AS ',
                   'SELECT mail_id, item_guid, item_template, receiver FROM ', @a, '.`mail_items`');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;
SET @sql := CONCAT('CREATE TEMPORARY TABLE custody_diff_mail_items_b AS ',
                   'SELECT mail_id, item_guid, item_template, receiver FROM ', @b, '.`mail_items`');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SET @sql := CONCAT('CREATE TEMPORARY TABLE custody_diff_item_instance_a AS ',
                   'SELECT guid, owner_guid FROM ', @a, '.`item_instance`');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;
SET @sql := CONCAT('CREATE TEMPORARY TABLE custody_diff_item_instance_b AS ',
                   'SELECT guid, owner_guid FROM ', @b, '.`item_instance`');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

-- auction.time is expireTime = time(NULL) + etime, a wall-clock timestamp set at
-- creation (identical between custody on/off; only clock drift between clone runs
-- differs). Normalize it out by default exactly like the mail times, so an
-- unpinned-clock run does not false-positive every created auction. AUCTION_DIFF
-- joins this column with <=> so NULL matches NULL when normalized.
SET @auction_time_col := IF(@compare_absolute_auction_times = 1, 'time', 'CAST(NULL AS UNSIGNED) AS time');
SET @auction_cols := CONCAT('id, houseid, itemguid, item_template, item_count, item_randompropertyid, ',
                            'itemowner, buyoutprice, ', @auction_time_col, ', buyguid, lastbid, startbid, deposit');
SET @sql := CONCAT('CREATE TEMPORARY TABLE custody_diff_auction_a AS SELECT ', @auction_cols, ' FROM ', @a, '.`auction`');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;
SET @sql := CONCAT('CREATE TEMPORARY TABLE custody_diff_auction_b AS SELECT ', @auction_cols, ' FROM ', @b, '.`auction`');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SELECT 'MAIL_TIME_WINDOW_A: inspect when absolute mail times are normalized out' AS section;
SET @sql := CONCAT('SELECT COUNT(*) AS rows_seen, MIN(deliver_time) AS min_deliver_time, MAX(deliver_time) AS max_deliver_time, ',
                   'MIN(CAST(expire_time AS SIGNED) - CAST(deliver_time AS SIGNED)) AS min_expire_delay, ',
                   'MAX(CAST(expire_time AS SIGNED) - CAST(deliver_time AS SIGNED)) AS max_expire_delay ',
                   'FROM ', @a, '.`mail`');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SELECT 'MAIL_TIME_WINDOW_B: inspect when absolute mail times are normalized out' AS section;
SET @sql := CONCAT('SELECT COUNT(*) AS rows_seen, MIN(deliver_time) AS min_deliver_time, MAX(deliver_time) AS max_deliver_time, ',
                   'MIN(CAST(expire_time AS SIGNED) - CAST(deliver_time AS SIGNED)) AS min_expire_delay, ',
                   'MAX(CAST(expire_time AS SIGNED) - CAST(deliver_time AS SIGNED)) AS max_expire_delay ',
                   'FROM ', @b, '.`mail`');
PREPARE stmt FROM @sql; EXECUTE stmt; DEALLOCATE PREPARE stmt;

SELECT 'CHARACTERS_DIFF: zero rows means pass' AS section;
SELECT 'A_MINUS_B' AS diff_side, a.guid, a.money
FROM custody_diff_characters_a a
LEFT JOIN custody_diff_characters_b b
  ON b.guid = a.guid AND b.money = a.money
WHERE b.guid IS NULL
UNION ALL
SELECT 'B_MINUS_A' AS diff_side, b.guid, b.money
FROM custody_diff_characters_b b
LEFT JOIN custody_diff_characters_a a
  ON a.guid = b.guid AND a.money = b.money
WHERE a.guid IS NULL
ORDER BY guid, diff_side;

SELECT 'MAIL_DIFF: zero rows means pass; deliver/expire normalized unless @compare_absolute_mail_times=1' AS section;
SELECT diff_side, receiver, sender, messageType, stationery, money, cod, subject, body,
       has_items, checked, deliver_time, expire_time, expire_delay, delta_count
FROM (
    SELECT 'A_MINUS_B' AS diff_side, a.*, a.row_count - COALESCE(b.row_count, 0) AS delta_count
    FROM (
        SELECT receiver, sender, messageType, stationery, money, cod, subject, body, has_items, checked,
               deliver_time, expire_time, expire_delay, COUNT(*) AS row_count
        FROM custody_diff_mail_a
        GROUP BY receiver, sender, messageType, stationery, money, cod, subject, body, has_items, checked,
                 deliver_time, expire_time, expire_delay
    ) a
    LEFT JOIN (
        SELECT receiver, sender, messageType, stationery, money, cod, subject, body, has_items, checked,
               deliver_time, expire_time, expire_delay, COUNT(*) AS row_count
        FROM custody_diff_mail_b
        GROUP BY receiver, sender, messageType, stationery, money, cod, subject, body, has_items, checked,
                 deliver_time, expire_time, expire_delay
    ) b
      ON b.receiver = a.receiver
     AND b.sender = a.sender
     AND b.messageType = a.messageType
     AND b.stationery = a.stationery
     AND b.money = a.money
     AND b.cod = a.cod
     AND b.subject <=> a.subject
     AND b.body <=> a.body
     AND b.has_items = a.has_items
     AND b.checked = a.checked
     AND b.deliver_time <=> a.deliver_time
     AND b.expire_time <=> a.expire_time
     AND b.expire_delay <=> a.expire_delay
    WHERE a.row_count <> COALESCE(b.row_count, 0)
    UNION ALL
    SELECT 'B_MINUS_A' AS diff_side, b.*, b.row_count - COALESCE(a.row_count, 0) AS delta_count
    FROM (
        SELECT receiver, sender, messageType, stationery, money, cod, subject, body, has_items, checked,
               deliver_time, expire_time, expire_delay, COUNT(*) AS row_count
        FROM custody_diff_mail_b
        GROUP BY receiver, sender, messageType, stationery, money, cod, subject, body, has_items, checked,
                 deliver_time, expire_time, expire_delay
    ) b
    LEFT JOIN (
        SELECT receiver, sender, messageType, stationery, money, cod, subject, body, has_items, checked,
               deliver_time, expire_time, expire_delay, COUNT(*) AS row_count
        FROM custody_diff_mail_a
        GROUP BY receiver, sender, messageType, stationery, money, cod, subject, body, has_items, checked,
                 deliver_time, expire_time, expire_delay
    ) a
      ON a.receiver = b.receiver
     AND a.sender = b.sender
     AND a.messageType = b.messageType
     AND a.stationery = b.stationery
     AND a.money = b.money
     AND a.cod = b.cod
     AND a.subject <=> b.subject
     AND a.body <=> b.body
     AND a.has_items = b.has_items
     AND a.checked = b.checked
     AND a.deliver_time <=> b.deliver_time
     AND a.expire_time <=> b.expire_time
     AND a.expire_delay <=> b.expire_delay
    WHERE b.row_count <> COALESCE(a.row_count, 0)
) d
ORDER BY receiver, money, subject, diff_side;

-- Diagnostic: MAIL_ITEMS_DIFF joins mail_id directly (per the idle MAX(mail.id)
-- snapshot precondition). If MAIL_DIFF is clean but this section is NOT, mail.id
-- drifted between the clones -- re-verify the idle-snapshot precondition.
SELECT 'MAIL_ITEMS_DIFF: zero rows means pass (clean MAIL_DIFF + dirty here => mail.id drift)' AS section;
SELECT diff_side, mail_id, item_guid, item_template, receiver, delta_count
FROM (
    SELECT 'A_MINUS_B' AS diff_side, a.*, a.row_count - COALESCE(b.row_count, 0) AS delta_count
    FROM (
        SELECT mail_id, item_guid, item_template, receiver, COUNT(*) AS row_count
        FROM custody_diff_mail_items_a
        GROUP BY mail_id, item_guid, item_template, receiver
    ) a
    LEFT JOIN (
        SELECT mail_id, item_guid, item_template, receiver, COUNT(*) AS row_count
        FROM custody_diff_mail_items_b
        GROUP BY mail_id, item_guid, item_template, receiver
    ) b
      ON b.mail_id = a.mail_id
     AND b.item_guid = a.item_guid
     AND b.item_template = a.item_template
     AND b.receiver = a.receiver
    WHERE a.row_count <> COALESCE(b.row_count, 0)
    UNION ALL
    SELECT 'B_MINUS_A' AS diff_side, b.*, b.row_count - COALESCE(a.row_count, 0) AS delta_count
    FROM (
        SELECT mail_id, item_guid, item_template, receiver, COUNT(*) AS row_count
        FROM custody_diff_mail_items_b
        GROUP BY mail_id, item_guid, item_template, receiver
    ) b
    LEFT JOIN (
        SELECT mail_id, item_guid, item_template, receiver, COUNT(*) AS row_count
        FROM custody_diff_mail_items_a
        GROUP BY mail_id, item_guid, item_template, receiver
    ) a
      ON a.mail_id = b.mail_id
     AND a.item_guid = b.item_guid
     AND a.item_template = b.item_template
     AND a.receiver = b.receiver
    WHERE b.row_count <> COALESCE(a.row_count, 0)
) d
ORDER BY receiver, item_guid, diff_side;

SELECT 'ITEM_INSTANCE_DIFF: zero rows means pass' AS section;
SELECT 'A_MINUS_B' AS diff_side, a.guid, a.owner_guid
FROM custody_diff_item_instance_a a
LEFT JOIN custody_diff_item_instance_b b
  ON b.guid = a.guid AND b.owner_guid = a.owner_guid
WHERE b.guid IS NULL
UNION ALL
SELECT 'B_MINUS_A' AS diff_side, b.guid, b.owner_guid
FROM custody_diff_item_instance_b b
LEFT JOIN custody_diff_item_instance_a a
  ON a.guid = b.guid AND a.owner_guid = b.owner_guid
WHERE a.guid IS NULL
ORDER BY guid, diff_side;

SELECT 'AUCTION_DIFF: zero rows means pass' AS section;
SELECT diff_side, id, houseid, itemguid, item_template, item_count, item_randompropertyid,
       itemowner, buyoutprice, time, buyguid, lastbid, startbid, deposit, delta_count
FROM (
    SELECT 'A_MINUS_B' AS diff_side, a.*, a.row_count - COALESCE(b.row_count, 0) AS delta_count
    FROM (
        SELECT id, houseid, itemguid, item_template, item_count, item_randompropertyid,
               itemowner, buyoutprice, time, buyguid, lastbid, startbid, deposit, COUNT(*) AS row_count
        FROM custody_diff_auction_a
        GROUP BY id, houseid, itemguid, item_template, item_count, item_randompropertyid,
                 itemowner, buyoutprice, time, buyguid, lastbid, startbid, deposit
    ) a
    LEFT JOIN (
        SELECT id, houseid, itemguid, item_template, item_count, item_randompropertyid,
               itemowner, buyoutprice, time, buyguid, lastbid, startbid, deposit, COUNT(*) AS row_count
        FROM custody_diff_auction_b
        GROUP BY id, houseid, itemguid, item_template, item_count, item_randompropertyid,
                 itemowner, buyoutprice, time, buyguid, lastbid, startbid, deposit
    ) b
      ON b.id = a.id
     AND b.houseid = a.houseid
     AND b.itemguid = a.itemguid
     AND b.item_template = a.item_template
     AND b.item_count = a.item_count
     AND b.item_randompropertyid = a.item_randompropertyid
     AND b.itemowner = a.itemowner
     AND b.buyoutprice = a.buyoutprice
     AND b.time <=> a.time
     AND b.buyguid = a.buyguid
     AND b.lastbid = a.lastbid
     AND b.startbid = a.startbid
     AND b.deposit = a.deposit
    WHERE a.row_count <> COALESCE(b.row_count, 0)
    UNION ALL
    SELECT 'B_MINUS_A' AS diff_side, b.*, b.row_count - COALESCE(a.row_count, 0) AS delta_count
    FROM (
        SELECT id, houseid, itemguid, item_template, item_count, item_randompropertyid,
               itemowner, buyoutprice, time, buyguid, lastbid, startbid, deposit, COUNT(*) AS row_count
        FROM custody_diff_auction_b
        GROUP BY id, houseid, itemguid, item_template, item_count, item_randompropertyid,
                 itemowner, buyoutprice, time, buyguid, lastbid, startbid, deposit
    ) b
    LEFT JOIN (
        SELECT id, houseid, itemguid, item_template, item_count, item_randompropertyid,
               itemowner, buyoutprice, time, buyguid, lastbid, startbid, deposit, COUNT(*) AS row_count
        FROM custody_diff_auction_a
        GROUP BY id, houseid, itemguid, item_template, item_count, item_randompropertyid,
                 itemowner, buyoutprice, time, buyguid, lastbid, startbid, deposit
    ) a
      ON a.id = b.id
     AND a.houseid = b.houseid
     AND a.itemguid = b.itemguid
     AND a.item_template = b.item_template
     AND a.item_count = b.item_count
     AND a.item_randompropertyid = b.item_randompropertyid
     AND a.itemowner = b.itemowner
     AND a.buyoutprice = b.buyoutprice
     AND a.time <=> b.time
     AND a.buyguid = b.buyguid
     AND a.lastbid = b.lastbid
     AND a.startbid = b.startbid
     AND a.deposit = b.deposit
    WHERE b.row_count <> COALESCE(a.row_count, 0)
) d
ORDER BY id, diff_side;
