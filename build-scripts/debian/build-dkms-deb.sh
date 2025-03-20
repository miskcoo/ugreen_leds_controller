#!/usr/bin/bash

set -e
set -x

pkgver="0.3"
pkgname="led-ugreen-dkms"
drivername="led-ugreen"

mkdir -p $pkgname/DEBIAN

cat <<EOF > $pkgname/DEBIAN/control 
Package: $pkgname
Version: $pkgver
Architecture: amd64
Maintainer: Yuhao Zhou <miskcoo@gmail.com>
Depends: dkms
Homepage: https://github.com/miskcoo/ugreen_leds_controller
Description: UGREEN NAS LED driver
 A reverse-engineered LED driver of UGREEN NAS.
EOF

cat <<EOF > $pkgname/DEBIAN/postinst
#!/usr/bin/bash

dkms add -m $drivername -v $pkgver
dkms build -m $drivername -v $pkgver && dkms install -m $drivername -v $pkgver || true

EOF

cat <<EOF > $pkgname/DEBIAN/prerm
#!/usr/bin/bash

dkms remove -m $drivername -v $pkgver --all || true
EOF

chmod +x $pkgname/DEBIAN/postinst
chmod +x $pkgname/DEBIAN/prerm


# dkms files
mkdir -p $pkgname/usr/src/$drivername-$pkgver

kmod_files=(kmod/Makefile kmod/dkms.conf kmod/led-ugreen.c kmod/led-ugreen.h kmod/Makefile)
for f in ${kmod_files[@]}; do
    cp -rv $f $pkgname/usr/src/$drivername-$pkgver/
done

# change to root 
chown -R root:root $pkgname/

dpkg -b $pkgname

rm -rv $pkgname

