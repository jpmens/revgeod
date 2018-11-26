include config.mk

CFLAGS= -Wall -Werror
LDFLAGS=-lmicrohttpd -lcurl -llmdb $(LIBS)

ifneq ($(origin STATSDHOST), undefined)
	CFLAGS += -DSTATSD=\"$(STATSDHOST)\"
	LDFLAGS += -lstatsdclient
endif

OBJS = json.o geohash.o geo.o db.o uptime.o

all: revgeod lmdb-ll-look

revgeod: revgeod.c $(OBJS) Makefile
	$(CC) $(CFLAGS) -o revgeod revgeod.c $(OBJS) $(LDFLAGS)

lmdb-ll-look: lmdb-ll-look.c geohash.o json.o db.o
	$(CC) $(CFLAGS) -o lmdb-ll-look lmdb-ll-look.c geohash.o json.o db.o -llmdb

clean:
	rm -f *.o

clobber: clean
	rm -f revgeod lmdb-ll-look
