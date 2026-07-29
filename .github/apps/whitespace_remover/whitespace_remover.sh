# Required files
find -name '*.cpp' -print0 | xargs -r0 sed -e 's/[[:blank:]]\+$//' -i
find -name '*.h' -print0 | xargs -r0 sed -e 's/[[:blank:]]\+$//' -i

# Optional files - uncomment lines below to add them.
#find -name '*.txt' -print0 | xargs -r0 sed -e 's/[[:blank:]]\+$//' -i
#find -name '*.cmake' -print0 | xargs -r0 sed -e 's/[[:blank:]]\+$//' -i
