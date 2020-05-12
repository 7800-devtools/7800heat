#!/bin/sh
# makepackages.sh
#   apply the release.dat contents to the release text in various sources
#   and documents, and then generate the individual release packages.

RELEASE=$(cat release.dat)
ERELEASE=$(cat release.dat | sed 's/ /_/g')
YEAR=$(date +%Y)

dos2unix *.txt >/dev/null 2>&1
cat 7800heat.c | sed 's/define PROGNAME .*/define PROGNAME "7800heat v'"$RELEASE"'"/g' > 7800heat.c.new
mv 7800heat.c.new 7800heat.c
unix2dos 7800heat.c >/dev/null 2>&1

# cleanup
find . -name .\*.swp -exec rm '{}' \;
rm -fr packages
mkdir packages
make dist
cd packages

for OSARCH in linux@Linux osx@Darwin win@Windows ; do
        for BITS in x64 x86 ; do
                OS=$(echo $OSARCH | cut -d@ -f1)
                ARCH=$(echo $OSARCH| cut -d@ -f2)
		rm -fr 7800heat
		mkdir 7800heat
		cp -R ../samples 7800heat/
		cp ../*.txt 7800heat/
		if [ "$OS" = win ] ; then
			cp ../7800heat.Win32."$BITS".exe 7800heat/7800heat.exe
			zip -r 7800heat-$ERELEASE-$OS-$BITS.zip 7800heat
		else
			cp ../7800heat."$ARCH"."$BITS" 7800heat/7800heat
			tar --numeric-owner -cvzf 7800heat-$ERELEASE-$OS-$BITS.tar.gz 7800heat
		fi
		rm -fr 7800heat
	done
done

cd ..		
dos2unix *.txt >/dev/null 2>&1
