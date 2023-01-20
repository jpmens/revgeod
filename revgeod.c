/*
 * revgeod
 * Copyright (C) 2018-2019 Jan-Piet Mens <jp@mens.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <getopt.h>
#define MHD_PLATFORM_H
#include <microhttpd.h>
#include "geohash.h"
#include "utstring.h"
#include "json.h"
#include "geo.h"
#include "db.h"
#include "uptime.h"
#include "uthash.h"
#ifdef STATSD
# include "statsd/statsd-client.h"
# define SAMPLE_RATE 1.0
statsd_link *sd;
#endif
#include "version.h"

#define GEOHASH_PRECISION	9
#define JSON_SPACE		"    "

#ifdef MHD_HTTP_NOT_ACCEPTABLE
# define NOT_ACCEPTABLE MHD_HTTP_NOT_ACCEPTABLE
#else
# define NOT_ACCEPTABLE MHD_HTTP_METHOD_NOT_ACCEPTABLE
#endif

#if MHD_VERSION < 0x00097002
# define MHD_Result		int
#else
# define MHD_Result             enum MHD_Result
#endif


static char *apikey;
static char *api_provider;
static struct db *db;
struct MHD_Daemon *mhdaemon;

struct appdata {
	char *appname;
	unsigned long queries;
	UT_hash_handle hh_appname;

};
struct appdata *appcache = NULL;

struct statistics {
	time_t		launchtime;
	unsigned long	requests;		/* total requests */
	unsigned long	geofail;		/* geocoding failed */
	unsigned long	opencage;
	unsigned long	locationiq;
	unsigned long	lmdb;
	unsigned long	stats;			/* How often self called */
} st;

struct appdata *find_app(const char *appname)
{
        struct appdata *app = NULL;

        HASH_FIND(hh_appname, appcache, appname, strlen(appname), app);
        return (app);
}

static void catcher(int sig)
{
	if (sig == SIGINT) {
		MHD_stop_daemon(mhdaemon);
		revgeo_free();
		db_close(db);
#ifdef STATSD
		statsd_finalize(sd);
#endif
		exit(1);
	}
}

static void ignore_sigpipe()
{
	struct sigaction oldsig;
	struct sigaction sig;

	sig.sa_handler = &catcher;
	sigemptyset(&sig.sa_mask);
#ifdef SA_INTERRUPT
	sig.sa_flags = SA_INTERRUPT;	/* SunOS */
#else
	sig.sa_flags = SA_RESTART;
#endif
	if (0 != sigaction(SIGPIPE, &sig, &oldsig))
		fprintf(stderr,
		"Failed to install SIGPIPE handler: %s\n", strerror(errno));
}

#if 0
int print_kv(void *cls, enum MHD_ValueKind kind, const char *key, const char *value)
{
	printf("%s: %s\n", key, value);
	return (MHD_YES);
}
#endif

static int send_content(struct MHD_Connection *conn, const char *page,
		const char *content_type, int status_code)
{
	int ret;
	struct MHD_Response *response;
	response = MHD_create_response_from_buffer(strlen(page),
				(void*) page, MHD_RESPMEM_MUST_COPY);
	if (!response)
		return (MHD_NO);
	MHD_add_response_header(response, "Content-Type", content_type);
	ret = MHD_queue_response(conn, status_code, response);
	MHD_destroy_response(response);
	return (ret);
}

static int send_page(struct MHD_Connection *conn, const char *page, int status_code)
{
	return send_content(conn, page, "text/plain", status_code);
}

static int send_json(struct MHD_Connection *conn, JsonNode *json, double lat, double lon, char *geohash, char *source)
{
	struct MHD_Response *resp;
	int ret, status_code = MHD_HTTP_OK;
	char *js, *content_type = "application/json";

#ifdef STATSD
	char s_str[128];
	snprintf(s_str, sizeof(s_str), "response.%s", source);
	statsd_inc(sd, s_str, SAMPLE_RATE);
#endif

	fprintf(stderr, "%s %lf,%lf ", geohash, lat, lon);

	if ((js = json_stringify(json, JSON_SPACE)) == NULL) {
		char *page = "JSON kaputt";
		resp = MHD_create_response_from_buffer(strlen(page), (void *)page, MHD_RESPMEM_MUST_COPY);
		status_code = MHD_HTTP_INSUFFICIENT_STORAGE;
		content_type = "text/plain";
		fprintf(stderr, "Kaputt\n");
	} else {
		resp = MHD_create_response_from_buffer(strlen(js), (void *)js, MHD_RESPMEM_MUST_COPY);
		fprintf(stderr, "%s\n", js);
		free(js);
	}

	json_delete(json);
	free(geohash);

	if (!resp)
		return (MHD_NO);

	MHD_add_response_header(resp, "Content-type", content_type);
	ret = MHD_queue_response(conn, status_code, resp);
	MHD_destroy_response(resp);

	return (ret);
}

static int get_stats(struct MHD_Connection *connection)
{
	JsonNode *json = json_mkobject(), *counters = json_mkobject(), *apps = json_mkobject();
	char *js, uptimebuf[BUFSIZ];
	struct stat sbuf;
	struct appdata *a = NULL, *tmp;

	json_append_member(counters, "_whoami",		json_mkstring(__FILE__));
	json_append_member(counters, "_version",	json_mkstring(VERSION));
#ifdef STATSD
	json_append_member(counters, "_statsd",		json_mkbool(true));
#endif
	json_append_member(counters, "_mhdversion",	json_mkstring(MHD_get_version()));
	json_append_member(counters, "stats",		json_mknumber(++st.stats));
	json_append_member(counters, "requests",	json_mknumber(st.requests));
	json_append_member(counters, "geocode_failed",	json_mknumber(st.geofail));
	json_append_member(counters, "locationiq",	json_mknumber(st.locationiq));
	json_append_member(counters, "opencage",	json_mknumber(st.opencage));
	json_append_member(counters, "lmdb",		json_mknumber(st.lmdb));

	HASH_ITER(hh_appname, appcache, a, tmp) {
		json_append_member(apps, a->appname,	json_mknumber(a->queries));
	}

	uptime(time(0) - st.launchtime, uptimebuf, sizeof(uptimebuf));
	json_append_member(json, "stats",	counters);
	json_append_member(json, "apps",	apps);
	json_append_member(json, "uptime",	json_mknumber(time(0) - st.launchtime));
	json_append_member(json, "uptime_s",	json_mkstring(uptimebuf));
	json_append_member(json, "tst",		json_mknumber(time(0)));
	json_append_member(json, "db_path",	json_mkstring(db_getpath(db)));
	json_append_member(json, "db_entries",	json_mknumber(db_numentries(db)));

	if (stat(LMDB_DATABASE"/data.mdb", &sbuf) == 0) {
		json_append_member(json, "db_size",	json_mknumber(sbuf.st_size));
	}

	if ((js = json_stringify(json, JSON_SPACE)) != NULL) {
		int ret = send_content(connection, js, "application/json", MHD_HTTP_OK);
		free(js);
		json_delete(json);
		return (ret);
	}

	json_delete(json);

	return send_content(connection, "{}", "application/json", MHD_HTTP_OK);

}

static int get_reversegeo(struct MHD_Connection *connection)
{
	double lat, lon;
	const char *s_lat, *s_lon, *s_app;
	JsonNode *json, *obj;
	char *geohash, *ap, *source, apbuf[8192];
	static UT_string *addr, *rawdata, *locality, *cc;
	struct timeval t_stop_oc, t_start_oc;
	struct appdata *app = NULL;
#ifdef STATSD
	double opencage_millis;
#endif

	// GET /rev?format=json&lat=48.85833&lon=3.29513&zoom=18&addressdetails=1

#ifdef STATSD
	statsd_inc(sd, "queries.incoming", SAMPLE_RATE);
#endif
	st.requests++;


	/*
	 * Were we given an application name? Then find it in the hash list and
	 * increment its query counter.
	 */

	s_app = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "app");
	if (s_app && *s_app) {
		if ((app = find_app(s_app)) == NULL) {
			app = malloc(sizeof(struct appdata));
			app->appname = strdup(s_app);
			app->queries = 1UL;
			HASH_ADD_KEYPTR(hh_appname, appcache, app->appname, strlen(app->appname), app);
		} else {
			app->queries++;
		}
	}


	s_lat = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "lat");
	s_lon = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "lon");
	if (!s_lat || !s_lon || !*s_lat || !*s_lon) {
		return send_page(connection, "One of lat or lon missing", NOT_ACCEPTABLE);
	}

	lat = atof(s_lat);
	lon = atof(s_lon);
	if ((geohash = geohash_encode(lat, lon, GEOHASH_PRECISION)) == NULL) {
		return send_page(connection, "Cannot encode to geohash", NOT_ACCEPTABLE);
	}

	utstring_renew(addr);
	utstring_renew(rawdata);
	utstring_renew(locality);
	utstring_renew(cc);

	json = json_mkobject();
	obj = json_mkobject();

	ap = "unknown address";

	/*
	 * check LMDB
	 * we expect a string which contains a JSON object.
	 */

	source = "lmdb";
	if (db_get(db, geohash, apbuf, sizeof(apbuf)) > 0) {
		JsonNode *address_obj;

		if ((address_obj = json_decode(apbuf)) == NULL) {
			fprintf(stderr, "Can't decode JSON apbuf [%s]\n", apbuf);
			json_append_member(obj, "village", json_mkstring("unknown"));
			json_append_member(obj, "locality", json_mkstring("unknown"));
			json_append_member(obj, "cc", json_mkstring("??"));
		} else {
			static char *elems[] = { "village", "locality", "cc", NULL }, **e;
			JsonNode *el;

			for (e = &elems[0]; e && *e; e++) {
				if ((el = json_find_member(address_obj, *e)) != NULL) {
					if (el->tag == JSON_STRING) {
						json_append_member(obj, *e, json_mkstring(el->string_));
					}
				}
			}

			json_delete(address_obj);
		}
		json_append_member(obj, "s", json_mkstring(source));
		json_append_member(json, "address", obj);
		st.lmdb++;
		return send_json(connection, json, lat, lon, geohash, source);
	}

	/*
	 * We haven't found entry in our database, so go out and obtain the
	 * reverse geo-encoding and store it here in LMDB.
	 */

	source = api_provider;
	gettimeofday(&t_start_oc, NULL);
	if (strcmp(api_provider,"opencage") == 0){
		st.opencage++;
	}else{
		st.locationiq++;
	}

	if (revgeo_getdata(apikey, api_provider, lat, lon, addr, rawdata, locality, cc) == true) {
		JsonNode *address_obj = json_mkobject();
		char *js;
		ap = UB(addr);

		/* Store the full address as JSON in LMDB (JSON takes care of UTF-8) */

		json_append_member(address_obj, "village",	json_mkstring(ap));
		json_append_member(address_obj, "locality",	json_mkstring(UB(locality)));
		json_append_member(address_obj, "cc",		json_mkstring(UB(cc)));

		if ((js = json_stringify(address_obj, JSON_SPACE)) != NULL) {
			printf("[[%s]]\n", js);
			if (db_put(db, geohash, js) != 0) {
				fprintf(stderr, "Cannot db_put: ");
				perror("");
			}
			free(js);
		}
		json_delete(address_obj);

	} else {
		st.geofail++;
	}

	gettimeofday(&t_stop_oc, NULL);

#ifdef STATSD
	opencage_millis = (double)(t_stop_oc.tv_usec - t_start_oc.tv_usec) / 1000
			+ (double)(t_stop_oc.tv_sec - t_start_oc.tv_sec) * 1000;

	printf("opencage_millis %f\n", opencage_millis);
	statsd_timing(sd, "response.opencage.duration", (long)opencage_millis);
#endif

	json_append_member(obj, "village",	json_mkstring(ap));
	json_append_member(obj, "locality",	json_mkstring(UB(locality)));
	json_append_member(obj, "cc",		json_mkstring(UB(cc)));
	json_append_member(obj, "s",		json_mkstring(source));
	json_append_member(json, "address", obj);
	return send_json(connection, json, lat, lon, geohash, source);
}

/* Ceci est very memory heavy ... */
static JsonNode *jarray = NULL;

static int enumhelper(int keylen, char *key, int datalen, char *data)
{
	JsonNode *o = json_mkobject(), *j;
	/* WARNING: `key' is not 0-terminated in db */
	char geohash[12], **t;
	int glen = (keylen > sizeof(geohash) - 1) ? sizeof(geohash) - 1 : keylen;
	static char *tags[] = { "village", "locality", "cc", NULL };
	GeoCoord g;

	memcpy(geohash, key, glen);
	geohash[glen] = 0;

	g = geohash_decode(geohash);

	json_append_member(o, "lat",		json_mknumber(g.latitude));
	json_append_member(o, "lon",		json_mknumber(g.longitude));
	json_append_member(o, "ghash",		json_mkstring(geohash));


	/* Copy the string elements from the JSON in `data' into the new object */
	if ((j = json_decode(data)) != NULL) {
		for (t = tags; t && *t; t++) {
			JsonNode *n;

			if ((n = json_find_member(j, *t)) != NULL) {
				if (n->tag == JSON_STRING) {
					json_append_member(o, *t, json_mkstring(n->string_));
				}
			}
		}
		json_delete(j);
	}

	json_append_element(jarray, o);
	return (0);
}

static int get_dumpall(struct MHD_Connection *connection)
{
	char *js;

	jarray = json_mkarray();

	db_enum(db, enumhelper);

	if ((js = json_stringify(jarray, JSON_SPACE)) != NULL) {
		int ret = send_content(connection, js, "application/json", MHD_HTTP_OK);
		free(js);
		json_delete(jarray);
		return (ret);
	}

	json_delete(jarray);

	return send_content(connection, "[]", "application/json", MHD_HTTP_OK);

}

/*
 * Delete (kill) a geohash from the database.
 * Yes, this should be a DELETE method...
 */

static int get_kill(struct MHD_Connection *connection)
{
	const char *geohash;
	char *txt;
	int rc;

	geohash = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "geohash");
	if (!geohash || !*geohash) {
		return send_page(connection, "Missing geohash", 422);
	}

	rc = db_del(db, (char *)geohash);
	txt = rc == 0 ? "deleted" : "Not deleted";

	return send_page(connection, txt, (rc == 0) ? 200 : 404);
}


static int get_lookup(struct MHD_Connection *connection)
{
	const char *geohash;
	char apbuf[8192];

	geohash = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "geohash");
	if (!geohash || !*geohash) {
		return send_page(connection, "Missing geohash", 422);
	}

	if (db_get(db, (char *)geohash, apbuf, sizeof(apbuf)) < 0) {
		return send_page(connection, "not found", 404);
	}

	return send_content(connection, apbuf, "application/json", MHD_HTTP_OK);
}

#if 0
int jp_queryp(void *cls, enum MHD_ValueKind kind, const char *key, const char *value)
{
	printf("param: %s=[%s]\n", key, value);
	return (MHD_YES);
}
#endif

MHD_Result handle_connection(void *cls, struct MHD_Connection *connection,
	const char *url, const char *method, const char *version,
	const char *upload_data, size_t *upload_data_size, void **con_cls)
{
	// fprintf(stderr,"%s %s %s\n", method, url, version);
	// MHD_get_connection_values(connection, MHD_HEADER_KIND, &print_kv, NULL);

	if (strcmp(method, "GET") != 0) {
		return (send_page(connection, "not allowed", MHD_HTTP_METHOD_NOT_ALLOWED));
	}

	// n = MHD_get_connection_values(connection, MHD_GET_ARGUMENT_KIND, jp_queryp, NULL);

	if (strcmp(url, "/rev") == 0) {
		return get_reversegeo(connection);
	}

	if (strcmp(url, "/stats") == 0) {
		return get_stats(connection);
	}

	if (strcmp(url, "/dump") == 0) {
		return get_dumpall(connection);
	}

	if (strcmp(url, "/lookup") == 0) {
		return get_lookup(connection);
	}

	if (strcmp(url, "/kill") == 0) {
		return get_kill(connection);
	}

	return send_page(connection, "four-oh-four", MHD_HTTP_NOT_FOUND);
}

int main(int argc, char **argv)
{
	struct sockaddr_in sad;
	char *s_ip, *s_port;
	unsigned short port;
	int ch;

#ifdef STATSD
	sd = statsd_init_with_namespace(STATSD, 8125, "revgeo");
#endif

	if ((s_ip = getenv("REVGEO_IP")) == NULL)
		s_ip = LISTEN_HOST;
	if ((s_port = getenv("REVGEO_PORT")) == NULL)
		s_port = LISTEN_PORT;
	port = atoi(s_port);

	while ((ch = getopt(argc, argv, "v")) != EOF) {
		switch (ch) {
			case 'v':
				printf("revgeod %s\n", VERSION);
				exit(0);
			default:
				fprintf(stderr, "Usage: %s [-v]\n", *argv);
				exit(2);
		}
	}

	argc -= optind;
	argv += optind;


	if ((apikey = getenv("OPENCAGE_APIKEY")) == NULL) {
		fprintf(stderr, "OPENCAGE_APIKEY is missing in environment. Trying LOCATIONIQ_APIKEY\n");
		if ((apikey = getenv("LOCATIONIQ_APIKEY")) == NULL) {
			fprintf(stderr, "LOCATIONIQ_APIKEY is missing in environment. Exiting...\n");
			exit(1);
		}else{
			api_provider = "locationiq";
		}	
	}else{
		api_provider = "opencage";
	}


	memset(&sad, 0, sizeof (struct sockaddr_in));
	sad.sin_family = AF_INET;
	sad.sin_port = htons(port);
	if (inet_pton(AF_INET, s_ip, &(sad.sin_addr.s_addr)) != 1) {
		perror("Can't parse IP");
		exit(2);
	}

	st.launchtime = time(0);

	revgeo_init();

	db = db_open(LMDB_DATABASE, NULL, false);
	if (db == NULL) {
		perror(LMDB_DATABASE);
		return (1);
	}

	ignore_sigpipe();
	signal(SIGINT, catcher);

	mhdaemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, 0, NULL, NULL,
		&handle_connection, NULL,
		MHD_OPTION_SOCK_ADDR, &sad,
		MHD_OPTION_END);

	if (mhdaemon == NULL) {
		perror("Cannot initialize HTTP daemon");
		return (1);
	}

	while (1) {
		sleep(60);
	}

	MHD_stop_daemon(mhdaemon);
	db_close(db);
	revgeo_free();
#ifdef STATSD
	statsd_finalize(sd);
#endif
	return (0);
}

