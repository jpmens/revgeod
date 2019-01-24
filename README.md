NAME
====

revgeod - reverse-geo lookup daemon

revgeoc - lookup client for revgeod

SYNOPSIS
========

revgeod \[-v\]

revgeoc stats|dump|lookup|kill|test

DESCRIPTION
===========

*revgeod* is a reverse Geo lookup daemon thing, accessible via HTTP and backed via [OpenCage](https://opencagedata.com), our geocoder of choice. You'll need an OpenCage API key exported into the environment, and you can specify *revgeod*'s listen IP address and port which default to `127.0.0.1` and `8865` respectively.

The (curently hardcoded) *geocache* directory must exist and be writeable by the owner of the *rungeod* process; that's where the LMDB database is stored. *revgeod* caches OpenCage's responses (they explicitly permit this):

*revgeoc* is the client program which speaks HTTP to *revgeod*.

EXAMPLE
=======

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

A second query for the same location would respond with `lmdb` instead of `opencage` as the source, indicating it's been cached.

ENDPOINTS
=========

All *revgeod* API endpoints are obtained via GET requests, and the client program *revgeoc* uses the same words as its commands.

`rev`
-----

The `/rev` endpoint is used to perform a reverse-geo lookup and cache the positive result. This endpoint supports the following query parameters:

-   `lat=` specify the latitude as a decimal (mandatory)
-   `lon=` specify the longitude as a decimal (mandatory)
-   `app=` specifies an "application" for which query statistics should be collected (see *statistics* below) (optional)

`stats`
-------

*revgeod* provides statistics on its `/stats` endpoint, and it collects counters by *application* if the `app` query parameter is specified during lookups:

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

`dump`
------

The `/dump` endpoint produces a full dump of the underling database in JSON format as an array of objects, each containing a *geohash*, the cached address information, and *lat* and *lon* elements which are the latitude and longitude respectively which have been decoded from the entries' *geohash*. Note that this means that the values are not those from which the entry originally resulted.

`lookup`
--------

This endpoint expects `geohash` query parameter with a *geohash* of precision 8; the key is looked up in the database and the JSON data or HTTP status code 404 are returned.

`kill`
------

Similarly to `lookup`, `/kill` also expects a *geohash* and removes it from the database.

OPTIONS
=======

*revgeod* understands the following global options.

-v  
show version information and exit

ENVIRONMENT
===========

`revgeo_verbose`  
if this variable is set when *revgeoc* starts, the program displays received HTTP headers

`OPENCAGE_APIKEY`  
this mandatory variable must be set in *revgeod*'s environment for it to do reverse geo lookups.

`REVGEO_HOST`  
optionally sets the listen address for *revgeod*; defaults to `127.0.0.1` and we strongly recommend this is not changed to anything other than a loopback address.

`REVGEO_PORT`  
optionally sets the TCP listen port to something other than the default `8865`.

REQUIREMENTS
============

rhel/centos
-----------

    yum install lmdb

debian
------

    apt-get install  liblmdb-dev lmdb-utils curl libcurl3

macos
-----

    brew install curl
    brew install jpmens/brew/revgeod

This is documented [here](https://github.com/jpmens/homebrew-brew), and the homebrew version is typically kept in sync with this version.

all
---

-   [libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/)
-   [statsd-c-client](https://github.com/romanbsd/statsd-c-client) (optional)

CREDITS
=======

-   `json.[ch]` by Joseph A. Adams.
-   [uthash](https://troydhanson.github.io/uthash/), by Troy D. Hanson.
-   [utstring](https://troydhanson.github.io/uthash/utstring.html), by Troy D. Hanson.

AVAILABILITY
============

<https://github.com/jpmens/revgeod>

AUTHOR
======

Jan-Piet Mens <https://jpmens.net>
