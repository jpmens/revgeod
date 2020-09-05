#!/bin/sh

set -e

umask 022

tempdir=$(mktemp -d /tmp/ot-XXX)

make install DESTDIR=$tempdir

install -D README.md $tempdir/usr/share/doc/revgeod/README.md
install -D etc/revgeod.service $tempdir/usr/share/doc/revgeod/revgeod.service

name="revgeod"
# add -0 to indicate "not in Debian" as per Roger's suggestion
version="$(awk 'NR==1 {print $NF;}' version.h | sed -e 's/"//g' )-0-deb$(cat /etc/debian_version)"
arch=$(uname -m)

case $arch in
	armv7l) arch=armhf;;
esac

debfile="${name}_${version}_${arch}.deb"

rm -f "${debfile}"

fpm -s dir \
        -t deb \
        -n ${name} \
        -v ${version} \
        --vendor "JP Mens" \
        -a "${arch}" \
        --maintainer "jp@mens.de" \
        --description "A reverse Geo lookup service written in C, accessible via HTTP and backed by OpenCage and LMDB" \
        --license "https://github.com/jpmens/revgeod/blob/master/LICENSE" \
        --url "https://github.com/jpmens/revgeod" \
        -C $tempdir \
        -p ${debfile} \
        -d "libcurl4" \
        -d "curl" \
        -d "liblmdb0" \
        -d "libmicrohttpd12" \
	--config-files usr/local/etc/default/revgeod \
	--post-install etc/debian/postinst \
        usr var 

echo "${debfile}" > package.name
rm -rf "${tempdir}"
