# Script to test loot template merge
import MySQLdb

# MySQL server
server = "localhost"
# Database
database = "mangos2"
# Username
username = "mangos"
# Password
password = "mangos"

# Open database connection
db = MySQLdb.connect(host=server, user=username, password=password, database=database)
cur = db.cursor()

loot_tables = ["creature_loot_template", "disenchant_loot_template", "fishing_loot_template", "gameobject_loot_template",
               "item_loot_template", "mail_loot_template", "milling_loot_template", "pickpocketing_loot_template",
               "prospecting_loot_template", "reference_loot_template", "skinning_loot_template", "spell_loot_template"]

lootTypeId = 1
origTotal = 0
with db:
  for loot_table in loot_tables:
    # Check that the correct number of rows from the original table have been merged into loot_template
    cur.execute(db.escape_string("SELECT COUNT(entry) FROM `%s`" % loot_table))
    origCount = cur.fetchone()[0]
    cur.execute(db.escape_string("SELECT COUNT(entry) FROM loot_template WHERE lootTypeId=%s" % lootTypeId))
    mergedCount = cur.fetchone()[0]
    if origCount != mergedCount:
      print("Mismatch for table "+loot_table)
      print(loot_table + " original count = " + str(origCount))
      print("loot_template merged count = " + str(mergedCount))
      exit()
    origTotal = origTotal + origCount
    lootTypeId = lootTypeId + 1

  # Check that the total number of rows matches the sum of all the original tables
  cur.execute(db.escape_string("SELECT COUNT(entry) FROM loot_template"))
  mergedTotal = cur.fetchone()[0]
  if origTotal != mergedTotal:
    print("Mismatch in total no. of rows!")
    exit()

print("No errors found!")
