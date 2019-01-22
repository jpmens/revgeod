#!/bin/sh

default=/usr/local/etc/default/revgeod

if [ -f $default ]; then
	. /usr/local/etc/default/revgeod

	if [ -n "$OPENCAGE_APIKEY" ]; then
		exec /usr/local/sbin/revgeod
	else
		echo "$0: OPENCAGE_APIKEY is missing from $default; exiting"
		exit 2
	fi
else
	echo "$0: cannot read default file at $default; exiting"
	exit 2
fi

