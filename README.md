


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
