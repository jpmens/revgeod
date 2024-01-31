/*
 * revgeod
 * Copyright (C) 2018-2024 Jan-Piet Mens <jp@mens.de>
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
				continue;
			}
			printf("%s %lf %lf %s\n", geohash, lat, lon, json_stringify(obj, NULL));
			if (obj != NULL) {
				json_delete(obj);
			}
		}
	}

	db_close(db);
	fclose(fp);
	return (0);
}
