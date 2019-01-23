# revgeod

A reverse Geo lookup thing, accessible via HTTP and backed via [OpenCage](https://opencagedata.com), our geocoder of choice. You'll need an API key exported into the environment, and you can specify _revgeod_'s listen IP address and port which default to `127.0.0.1` and `8865` respectively.

```bash
#!/bin/sh

export OPENCAGE_APIKEY="xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
export REVGEO_IP=127.0.0.1
export REVGEO_PORT=8865

exec ./revgeod
```

The _geocache_ directory (currently hardcoded) must exist and be writeable; that's where the LMDB database is stored. _revgeod_ caches OpenCage's responses (they explicitly permit this):

![cache as long as you want](assets/airjp480.png)

## example

```bash
$ curl -i 'http://127.0.0.1:8865/rev?lat=48.85593&lon=2.29431'
HTTP/1.1 200 OK
Connection: Keep-Alive
Content-Length: 163
Content-type: application/json
Date: Wed, 23 Jan 2019 14:08:43 GMT

{
    "address": {
        "village": "4 r du Général Lambert, 75015 Paris, France",
        "locality": "Paris",
        "cc": "FR",
        "s": "opencage"
    }
}
```

A second query for the same location would respond with `lmdb` instead of `opencage` as the source, indicating it's been cached.

## query params

The following query parameters are supported on the `/rev` endpoint:

- `lat=` specify the latitude as a decimal (mandatory)
- `lon=` specify the longitude as a decimal (mandatory)
- `app=` specifies an "application" for which query statistics should be collected (see _statistics_ below) (optional)

## statistics

_revgeod_ provides statistics on its `/stats` endpoint:

```json
{
   "stats": {
      "_whoami": "revgeod.c",
      "_version": "0.1.8",
      "stats": 8,
      "requests": 13647,
      "geocode_failed": 9,
      "opencage": 13624,
      "lmdb": 23
   },
   "apps": {
      "recorder": 13,
      "clitest": 5,
      "jp0": 2
   },
   "uptime": 381258,
   "uptime_s": "4 days, 9 hours, 54 mins",
   "tst": 1544555424,
   "db_path": "/usr/local/var/revgeod/geocache/",
   "db_entries": 43756,
   "db_size": 7532544
}
```

## options

The following command-line switches are supported for _revgeod_:

* `-v`: show version and exit

The following keywords are supported in _revgeoc_:

* `stats`: connect to _revgeod_ on its compiled-in address/port to obtain and print statistics. Can also be specified as `-s` for backward compatibility.
* `dump`: dump content of database (all keys/values) to stdout, lines areprefixed by lat,lon; can also be specified as `-d` or `-D`
* `kill`: kill (i.e. delete) the specified geohash
* `lookup`: query the specified geohash

## requirements

### rhel/centos

```
yum install lmdb
```

### debian

```
apt-get install  liblmdb-dev lmdb-utils curl libcurl3
```

### macos

```
brew install curl
brew install jpmens/brew/revgeod
```

This is documented [here](https://github.com/jpmens/homebrew-brew), and the homebrew version is typically kept in sync with this version.


## blurbs

1. {"address":{"village":"unknown address","s":"opencage"}}


curl_easy_perform() failed: Timeout was reached: HTTP status code == 0
opencage_millis 4304.350000

2. {"address":{"village":"La Terre Noire, 77510 Sablonnières, France","s":"opencage"}}

3. {"address":{"village":"La Terre Noire, 77510 Sablonnières, France","s":"lmdb"}}


## valgrind

Thanks to [this post](https://blogs.kolabnow.com/2018/06/07/a-short-guide-to-lmdb) I learn that
> in order to run LMDB under Valgrind, the maximum mapsize must be smaller than half your available ram.


## requirements

* [libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/)
* [statsd-c-client](https://github.com/romanbsd/statsd-c-client) (optional)

## credits

* [uthash](https://troydhanson.github.io/uthash/), by Troy D. Hanson
* [utstring](https://troydhanson.github.io/uthash/utstring.html), by Troy D. Hanson

## author

Jan-Piet Mens `<jp()mens.de>`
