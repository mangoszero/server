# Required files

# Find all .cpp files and process them:
#  - Remove trailing whitespace at the end of lines (spaces or tabs)
#  - Replace every tab character with 4 spaces
find -name '*.cpp' -print0 | xargs -r0 sed -e 's/[[:blank:]]\+$//' -e 's/\t/    /g' -i

# Find all .h files and process them the same way:
#  - Remove trailing whitespace at the end of lines (spaces or tabs)
#  - Replace every tab character with 4 spaces
find -name '*.h' -print0 | xargs -r0 sed -e 's/[[:blank:]]\+$//' -e 's/\t/    /g' -i

# Optional files - uncomment lines below to add them.
#find -name '*.txt' -print0 | xargs -r0 sed -e 's/[[:blank:]]\+$//' -i
#find -name '*.cmake' -print0 | xargs -r0 sed -e 's/[[:blank:]]\+$//' -i
