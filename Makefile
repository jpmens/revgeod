include config.mk

CFLAGS= -Wall -Werror
LDFLAGS=-lmicrohttpd -lcurl -llmdb $(LIBS)

INSTALLDIR = /usr/local

ifneq ($(origin STATSDHOST), undefined)
	CFLAGS += -DSTATSD=\"$(STATSDHOST)\"
	CFLAGS += $(INC)
	LDFLAGS += -lstatsdclient
endif

CFLAGS += -DLMDB_DATABASE=\"$(LMDB_DATABASE)\"
CFLAGS += -DLISTEN_HOST=\"$(LISTEN_HOST)\"
CFLAGS += -DLISTEN_PORT=\"$(LISTEN_PORT)\"

OBJS = json.o geohash.o geo.o db.o uptime.o

all: revgeod lmdb-ll-look

revgeod: revgeod.c $(OBJS) Makefile version.h config.mk
	$(CC) $(CFLAGS) -o revgeod revgeod.c $(OBJS) $(LDFLAGS)

geo.o: geo.c json.h version.h

lmdb-ll-look: lmdb-ll-look.c geohash.o json.o db.o
	$(CC) $(CFLAGS) -o lmdb-ll-look lmdb-ll-look.c geohash.o json.o db.o -llmdb

clean:
	rm -f *.o

clobber: clean
	rm -f revgeod lmdb-ll-look

install: revgeod
	mkdir -p $(DESTDIR)$(INSTALLDIR)/sbin
	mkdir -p $(DESTDIR)/var/local/revgeod
	chown daemon $(DESTDIR)/var/local/revgeod
	chmod 755 $(DESTDIR)/var/local/revgeod
	install -m 755 revgeod $(DESTDIR)$(INSTALLDIR)/sbin/revgeod
	install -m 755 etc/revgeod.sh $(DESTDIR)$(INSTALLDIR)/sbin/revgeod.sh
	install -o daemon -m 640 etc/revgeod.default $(DESTDIR)$(INSTALLDIR)/etc/default/revgeod
