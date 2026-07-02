-- DebugVis labeled-marker pool.
--
-- Reserves a block of generic gameobject_template entries the debug visualizer
-- ring-allocates at runtime. Each labeled marker gets its own entry so the
-- client (which caches GO name/type per entry) shows that marker's verbose
-- tooltip. The model/colour is overridden per-instance via GAMEOBJECT_DISPLAYID,
-- so the template displayId here is only a harmless default.
--
-- Range 305000..305511 sits just above the current MAX(entry) (~300153), so
-- sGOStorage's index array grows only marginally.
--
-- type=10 = GAMEOBJECT_TYPE_GOOBER. This matters: the vanilla client only shows
-- a name tooltip / mouse-over highlight for *interactive* gameobjects. type=5
-- GENERIC (decorative doodads: auras, columns) render but never show a tooltip,
-- so the per-instance hover label would be invisible. GOOBER shows the name on
-- hover with a harmless click. All goober data fields are 0 (no lock, no quest,
-- no spell), so clicking does nothing meaningful. Safe to re-run.

DELETE FROM `gameobject_template` WHERE `entry` BETWEEN 305000 AND 305511;

-- Generate 305000..305511 (512 entries) without a recursive CTE so this stays
-- portable to MySQL 5.5+/MariaDB 5.5+ (WITH RECURSIVE needs MySQL 8.0/MariaDB
-- 10.2+). Three base-8 digit tables cross-join to 8*8*8 = 512 distinct offsets.
INSERT INTO `gameobject_template` (`entry`, `type`, `displayId`, `name`, `faction`, `flags`, `size`)
SELECT 305000 + d.n, 10, 263, 'DebugVis Marker', 0, 0, 1
FROM
(
    SELECT ones.n + eights.n * 8 + sixtyfours.n * 64 AS n
    FROM      (SELECT 0 AS n UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4 UNION ALL SELECT 5 UNION ALL SELECT 6 UNION ALL SELECT 7) ones
    CROSS JOIN (SELECT 0 AS n UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4 UNION ALL SELECT 5 UNION ALL SELECT 6 UNION ALL SELECT 7) eights
    CROSS JOIN (SELECT 0 AS n UNION ALL SELECT 1 UNION ALL SELECT 2 UNION ALL SELECT 3 UNION ALL SELECT 4 UNION ALL SELECT 5 UNION ALL SELECT 6 UNION ALL SELECT 7) sixtyfours
) AS d;
