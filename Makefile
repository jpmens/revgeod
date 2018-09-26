
# leave undefined if you don't want to use statsd
STATSDHOST="127.0.0.1"

CFLAGS= -Wall -Werror
LDFLAGS=-lmicrohttpd -lgnutls -lcurl -llmdb

ifneq ($(origin STATSDHOST), undefined)
	CFLAGS += -DSTATSD=\"$(STATSDHOST)\"
	LDFLAGS += -lstatsdclient
endif

OBJS = json.o geohash.o geo.o db.o

all: revgeod lmdb-ll-look

revgeod: revgeod.c $(OBJS) Makefile
	$(CC) $(CFLAGS) -o revgeod revgeod.c $(OBJS) $(LDFLAGS)

lmdb-ll-look: lmdb-ll-look.c geohash.o json.o db.o
	$(CC) $(CFLAGS) -o lmdb-ll-look lmdb-ll-look.c geohash.o json.o db.o -llmdb

clean:
	rm -f *.o

clobber: clean
	rm -f revgeod lmdb-ll-look
