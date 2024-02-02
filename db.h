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

#ifndef _DB_H_INCLUDED_
# define _DB_H_INCLUDED_


#include <lmdb.h>

#define LMDB_DB_SIZE	((size_t)10 * 1024 * 1024 * 1024)

struct db {
	MDB_env *env;
	MDB_dbi dbi;
};

struct db *db_open(char *path, char *dbname, int rdonly);
void db_close(struct db *);
int db_put(struct db *, char *ghash, char *payload);
long db_get(struct db *, char *key, char *buf, long buflen);
void db_dump(char *path, char *lmdbname);
void db_list(char *path, char *lmdbname, int (*func)(int, char *, int, char *));
int db_enum(struct db *db, int (*func)(int keylen, char *key, int datlen, char *data));
void db_load(char *path, char *lmdbname);
int db_del(struct db *db, char *keystr);
size_t db_numentries(struct db *db);
const char *db_getpath(struct db *db);
#endif
