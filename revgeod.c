/*
 * revgeod
 * Copyright (C) 2018 Jan-Piet Mens <jp@mens.de>
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
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#define MHD_PLATFORM_H
#include <microhttpd.h>
#include "geohash.h"
#include "utstring.h"
#include "json.h"
#include "geo.h"
#include "db.h"
#include "uptime.h"
#ifdef STATSD
# include <statsd/statsd-client.h>
# define SAMPLE_RATE 1.0
statsd_link *sd;
#endif

#define GEOHASH_PRECISION	8

#ifdef MHD_HTTP_NOT_ACCEPTABLE
# define NOT_ACCEPTABLE MHD_HTTP_NOT_ACCEPTABLE
#else
# define NOT_ACCEPTABLE MHD_HTTP_METHOD_NOT_ACCEPTABLE
#endif

static char *apikey;
static struct db *db;
struct MHD_Daemon *mhdaemon;

struct statistics {
	time_t		launchtime;
	unsigned long	requests;		/* total requests */
	unsigned long	geofail;		/* geocoding failed */
	unsigned long	opencage;
	unsigned long	lmdb;
	unsigned long	stats;			/* How often self called */
} st;

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

int print_kv(void *cls, enum MHD_ValueKind kind, const char *key, const char *value)
{
	printf("%s: %s\n", key, value);
	return (MHD_YES);
}

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

	if ((js = json_stringify(json, NULL)) == NULL) {
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
	JsonNode *json = json_mkobject(), *counters = json_mkobject();
	char *js, uptimebuf[BUFSIZ];

	json_append_member(counters, "_whoami",		json_mkstring(__FILE__));
#ifdef STATSD
	json_append_member(counters, "_statsd",		json_mkbool(true));
#endif
	json_append_member(counters, "stats",		json_mknumber(++st.stats));
	json_append_member(counters, "requests",	json_mknumber(st.requests));
	json_append_member(counters, "geocode_failed",	json_mknumber(st.geofail));
	json_append_member(counters, "opencage",	json_mknumber(st.opencage));
	json_append_member(counters, "lmdb",		json_mknumber(st.lmdb));

	uptime(time(0) - st.launchtime, uptimebuf, sizeof(uptimebuf));
	json_append_member(json, "stats",	counters);
	json_append_member(json, "uptime",	json_mknumber(time(0) - st.launchtime));
	json_append_member(json, "uptime_s",	json_mkstring(uptimebuf));
	json_append_member(json, "tst",		json_mknumber(time(0)));

	if ((js = json_stringify(json, NULL)) != NULL) {
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
	const char *s_lat, *s_lon;
	JsonNode *json, *obj;
	char *geohash, *ap, *source, apbuf[8192];
	static UT_string *addr, *rawdata, *locality, *cc;
	struct timeval t_stop_oc, t_start_oc;
	double opencage_millis;

	// GET /rev?format=json&lat=48.85833&lon=3.29513&zoom=18&addressdetails=1

#ifdef STATSD
	statsd_inc(sd, "queries.incoming", SAMPLE_RATE);
#endif
	st.requests++;

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

	source = "opencage";
	gettimeofday(&t_start_oc, NULL);
	st.opencage++;

	if (revgeo_getdata(apikey, lat, lon, addr, rawdata, locality, cc) == true) {
		JsonNode *address_obj = json_mkobject();
		char *js;
		ap = UB(addr);

		/* Store the full address as JSON in LMDB (JSON takes care of UTF-8) */

		json_append_member(address_obj, "village",	json_mkstring(ap));
		json_append_member(address_obj, "locality",	json_mkstring(UB(locality)));
		json_append_member(address_obj, "cc",		json_mkstring(UB(cc)));

		if ((js = json_stringify(address_obj, NULL)) != NULL) {
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

	opencage_millis = (double)(t_stop_oc.tv_usec - t_start_oc.tv_usec) / 1000
			+ (double)(t_stop_oc.tv_sec - t_start_oc.tv_sec) * 1000;

#ifdef STATSD
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

int jp_queryp(void *cls, enum MHD_ValueKind kind, const char *key, const char *value)
{
	printf("param: %s=[%s]\n", key, value);
	return (MHD_YES);
}

int handle_connection(void *cls, struct MHD_Connection *connection,
	const char *url, const char *method, const char *version,
	const char *upload_data, size_t *upload_data_size, void **con_cls)
{
	// printf("%s %s %s\n", method, url, version);
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
	return send_page(connection, "four-oh-four", MHD_HTTP_NOT_FOUND);
}

int main()
{
	struct sockaddr_in sad;
	char *s_ip, *s_port;
	unsigned short port;

#ifdef STATSD
	sd = statsd_init_with_namespace(STATSD, 8125, "revgeo");
#endif

	if ((apikey = getenv("OPENCAGE_APIKEY")) == NULL) {
		fprintf(stderr, "OPENCAGE_APIKEY is missing in environment\n");
		exit(1);
	}

	if ((s_ip = getenv("REVGEO_IP")) == NULL)
		s_ip = "127.0.0.1";
	if ((s_port = getenv("REVGEO_PORT")) == NULL)
		s_port = LISTEN_PORT;

	port = atoi(s_port);
	

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

