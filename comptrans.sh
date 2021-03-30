echo -n "Generating project file for translations..."

qmake -project -o translations.pro
echo "done"
echo >> translations.pro
echo -n "TRANSLATIONS += " >> translations.pro
awk '$1 !~ /^#/ { printf "translations/lprof_%s.ts ",$1 ; } END { printf "\n" }' translations/languages >> translations.pro

# Compile the po and ts format translations into the qm files that lprof
# requires to actually load its translations.

echo -n "Compiling po and ts files..."
lrelease translations.pro
mkdir data/translations
cp translations/*.qm data/translations/
rm -f translations/*.qm
rm -f translations.pro
echo "done"
