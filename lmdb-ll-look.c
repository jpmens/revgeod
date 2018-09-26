#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "geohash.h"
#include "json.h"
#include "db.h"

#define DBFILE			"data/geocache"
#define GEOHASH_PRECISION	8

int main(int argc, char **argv)
{
	struct db *db;
	FILE *fp;
	char buf[BUFSIZ], apbuf[BUFSIZ], *geohash;
	double lat = 0, lon = 0;

	if ((fp = fopen("data/lat-lon.csv", "r")) == NULL) {
		perror("data/lat-lon.csv");
		return (1);
	}

	if ((db = db_open(DBFILE, NULL, true)) == NULL) {
		perror(DBFILE);
		return (1);
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		char *village = "unknown";
		JsonNode *obj = NULL;

		if (buf[strlen(buf) - 1] == '\n')
			buf[strlen(buf)-1] = '0';

		if (sscanf(buf, "%lf,%lf", &lat, &lon) != 2) {
			continue;
		}
		geohash = geohash_encode(lat, lon, GEOHASH_PRECISION);
		if (db_get(db, geohash, apbuf, sizeof(apbuf)) > 0) {
			if ((obj = json_decode(apbuf)) == NULL) {
				fprintf(stderr, "Can't decode JSON ap\n");
			} else {
				village = obj->string_;
			}
		}
		printf("%s %lf %lf %s\n", geohash, lat, lon, village);
		if (obj != NULL) {
			json_delete(obj);
		}
	}

	db_close(db);
	fclose(fp);
	return (0);
}
