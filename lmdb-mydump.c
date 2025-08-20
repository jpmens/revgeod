/*
 * revgeod
 * Copyright (C) 2025 Jan-Piet Mens <jp@mens.de>
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
#include <stdlib.h>
#include "json.h"
#include "db.h"

int dumper(int keylen, char *key, int datlen, char *data)
{
	JsonNode *json;
	char *buf = malloc(datlen + 1), *js;

	memcpy(buf, data, datlen);
	buf[datlen] = 0;
	json = json_decode(buf);

	json = json_decode(buf);
	js = json_stringify(json, NULL);

	printf("%*.*s %s\n", keylen, keylen, key, js);
	free(js);
	free(buf);

	return 1;
}

int main(int argc, char **argv)
{
	struct db *db;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <lmdb-directory>\n", *argv);
		return (2);
	}

	if ((db = db_open(argv[1], NULL, true)) == NULL) {
		perror(argv[1]);
		return (1);
	}

	// db_dump(DBFILE, NULL);
	
	db_enum(db, dumper);

	db_close(db);
	return (0);
}
