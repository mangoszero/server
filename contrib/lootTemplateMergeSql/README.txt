#
# This code is part of MaNGOS. Contributor & Copyright details are in AUTHORS/THANKS.
#

Contents
loot_template_merge.sql     - MySQL commands to merge all the xxx_loot_template tables into a single loot_template table.
                              A new field, lootTypeId is added to differentiate entries from the various source tables.
                              Execute with: mysql -h localhost -u mangos --password=mangos -D mangos2 < loot_template_merge.sql
test_loot_template_merge.py - Python script to run some basic queries on the xxx_loot_template and loot_template tables to verify
                              that the merge was correct.

Requirements:
* Python 3.10 (at least tested with this version)
* The MySQLdb module 

Backup your data, before you change anything!
