#!/usr/bin/env python -B
# -*- coding: utf-8 -*-

from revgeod.geocoder import RevgeodGeocode
from collections import namedtuple
import sys

__author__    = 'Jan-Piet Mens <jp@mens.de>'
__copyright__ = 'Copyright 2018 Jan-Piet Mens'
__license__   = """GNU General Public License"""

Addr = namedtuple('Addr', 's lat lon cc locality village')

geocoder = RevgeodGeocode(host="127.0.0.1", port=8865)

for lat,lon in [ (48.85833, 3.29513), 
                 (49.39887, 8.68652) ]:

    a = geocoder.reverse_geocode(lat, lon)
    s = Addr(**a)

    print("{lat:10},{lon:10} {s:<10} {village}".format(
                    lat=lat, lon=lon, s=s.s,
                    village=s.village.encode('utf-8')))

    print(s.village.encode('utf-8'))
    sys.stdout.write(a['village'] + "\n")
