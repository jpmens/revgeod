include config.mk

CFLAGS= -Wall -Werror $(INC)
LDFLAGS=-lmicrohttpd -lcurl -llmdb $(LIBS)

INSTALLDIR = /usr/local

CFLAGS += -DLMDB_DATABASE=\"$(LMDB_DATABASE)\"
CFLAGS += -DLISTEN_HOST=\"$(LISTEN_HOST)\"
CFLAGS += -DLISTEN_PORT=\"$(LISTEN_PORT)\"

OBJS = json.o geohash.o geo.o db.o uptime.o 

ifneq ($(origin STATSDHOST), undefined)
	CFLAGS += -DSTATSD=\"$(STATSDHOST)\"
	CFLAGS += $(INC)
	OBJS += statsd/statsd-client.o
endif

all: revgeod lmdb-ll-look

revgeod: revgeod.c $(OBJS) Makefile version.h config.mk
	$(CC) $(CFLAGS) -o revgeod revgeod.c $(OBJS) $(LDFLAGS)

geo.o: geo.c json.h version.h

lmdb-ll-look: lmdb-ll-look.c geohash.o json.o db.o
	$(CC) $(CFLAGS) -o lmdb-ll-look lmdb-ll-look.c geohash.o json.o db.o -llmdb $(LIBS)

clean:
	rm -f *.o

clobber: clean
	rm -f revgeod lmdb-ll-look

install: revgeod
	[ -d $(DESTDIR)$(INSTALLDIR)/bin ] || ( mkdir -p $(DESTDIR)$(INSTALLDIR)/bin; chmod 755 $(DESTDIR)$(INSTALLDIR)/bin )
	[ -d $(DESTDIR)$(INSTALLDIR)/sbin ] || ( mkdir -p $(DESTDIR)$(INSTALLDIR)/sbin; chmod 755 $(DESTDIR)$(INSTALLDIR)/sbin )
	mkdir -p $(DESTDIR)/var/local/revgeod
	chmod 755 $(DESTDIR)/var/local/revgeod
	mkdir -p $(DESTDIR)$(INSTALLDIR)/share/man/man1 $(DESTDIR)$(INSTALLDIR)/share/man/man8
	chmod 755 $(DESTDIR)$(INSTALLDIR)/share/man/man1 $(DESTDIR)$(INSTALLDIR)/share/man/man8
	install -m 755 revgeoc $(DESTDIR)$(INSTALLDIR)/bin/revgeoc
	install -m 755 revgeod $(DESTDIR)$(INSTALLDIR)/sbin/revgeod
	mkdir -p $(DESTDIR)$(INSTALLDIR)/etc/default
	chmod 755 $(DESTDIR)$(INSTALLDIR)/etc/default
	install -m 640 etc/revgeod.default $(DESTDIR)$(INSTALLDIR)/etc/default/revgeod

	install -m 644 revgeod.1 $(DESTDIR)$(INSTALLDIR)/share/man/man1/revgeoc.1
	install -m 644 revgeod.1 $(DESTDIR)$(INSTALLDIR)/share/man/man8/revgeod.1

docs: revgeod.1 README.md

revgeod.1: revgeod.pandoc Makefile
	pandoc -s -w man+simple_tables -o $@ $<

README.md: revgeod.pandoc Makefile
	pandoc -s -w markdown_github+simple_tables $<  > $@
#	pandoc -s -w markdown+simple_tables $< | sed -n -e '4,$$p' > $@
