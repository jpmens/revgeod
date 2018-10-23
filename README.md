
# revgeod

A reverse Geo lookup thing, accessible via HTTP and backed via [OpenCage](https://opencagedata.com), our geocoder of choice. You'll need an API key exported into the environment, and you can specify _revgeod_'s listen IP address and port which default to `127.0.0.1` and `8865` respectively.

```bash
#!/bin/sh

export OPENCAGE_APIKEY="xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
export REVGEO_IP=127.0.0.1
export REVGEO_PORT=8865

mkdir -p data/geocache/

exec ./revgeod
```

The `data/geocache/` directory (currently hardcoded) must exist and be writeable; that's where the LMDB database is stored. _revgeod_ caches OpenCage's responses (they explicitly permit this):

![cache as long as you want](assets/airjp480.png)


## debian

```
apt-get install  liblmdb-dev lmdb-utils
```



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
* [statsd-c-client](https://github.com/romanbsd/statsd-c-client)
