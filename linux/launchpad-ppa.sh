#!/bin/bash

cd $(dirname $(realpath $0))

DATE=$(date -R)
VERSION="1.0.2-1"

for RELEASE in eoan disco cosmic bionic
do

cat >debian/changelog <<END
octopus ($VERSION~$RELEASE) $RELEASE; urgency=medium

  * Update to $VERSION

 -- Tom Kistner <tom@kistner.nu>  $DATE
END

debuild -S | tee debuild.log 2>&1
dput ppa:duncanthrax/octopus "$(perl -ne 'print $1 if /dpkg-genchanges --build=source >(.*)/' debuild.log)"
rm -f debuild.log

done
