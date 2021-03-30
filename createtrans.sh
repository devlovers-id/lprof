# create a temp project file to use to generate the ts files

echo -n "Generating project file for translations..."

qmake -project -o translations.pro
echo "done"
echo >> translations.pro
echo -n "TRANSLATIONS += " >> translations.pro
awk '$1 !~ /^#/ { printf "translations/lprof_%s.ts ",$1 ; } END { printf "\n" }' translations/languages >> translations.pro

# Update the editable po and ts translations with strings from the current
# sources.

echo -n "Updating translations..."
mkdir -p translations
lupdate translations.pro
echo "done"
echo "ts and po files output in translations/ "

# then remove the temp project file

rm -f translations.pro
