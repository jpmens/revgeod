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
#include <unistd.h>
#include <stdlib.h>
#include <syslog.h>
#include <assert.h>
#include <stdbool.h>
#include "db.h"

/*
 * `path' must be a directory; dbname is a named LMDB database; may be NULL.
 */

struct db *db_open(char *path, char *dbname, int rdonly)
{
	MDB_txn *txn = NULL;
	int rc;
	/* MDB_NOTLS is used to disable any LMDB internal thread related locking; */
	unsigned int flags = MDB_NOTLS, dbiflags = 0, perms = 0664;
	struct db *db;

	if ((db = malloc(sizeof (struct db))) == NULL)
		return (NULL);

	memset(db, 0, sizeof(struct db));

	if (rdonly) {
		flags |= MDB_RDONLY;
		perms = 0444;
		perms = 0664;
	} else {
		dbiflags = MDB_CREATE;
	}

	rc = mdb_env_create(&db->env);
	if (rc != 0) {
		syslog(LOG_ERR, "db_open: mdb_env_create: %s", mdb_strerror(rc));
		free(db);
		return (NULL);
	}

	mdb_env_set_mapsize(db->env, LMDB_DB_SIZE);

	rc = mdb_env_set_maxdbs(db->env, 10);
	if (rc != 0) {
		syslog(LOG_ERR, "db_open: mdb_env_set_maxdbs%s", mdb_strerror(rc));
		free(db);
		return (NULL);
	}

	rc = mdb_env_open(db->env, path, flags, perms);
	if (rc != 0) {
		syslog(LOG_ERR, "db_open: mdb_env_open: %s", mdb_strerror(rc));
		free(db);
		return (NULL);
	}

	/* Open a pseudo TX so that we can open DBI */

	mdb_txn_begin(db->env, NULL, flags, &txn);
	if (rc != 0) {
		syslog(LOG_ERR, "db_open: mdb_txn_begin: %s", mdb_strerror(rc));
		mdb_env_close(db->env);
		free(db);
		return (NULL);
	}

	rc = mdb_dbi_open(txn, dbname, dbiflags, &db->dbi);
	if (rc != 0) {
		syslog(LOG_ERR, "db_open: mdb_dbi_open for `%s': %s", dbname, mdb_strerror(rc));
		mdb_txn_abort(txn);
		mdb_env_close(db->env);
		free(db);
		return (NULL);
	}

	rc = mdb_txn_commit(txn);
	if (rc != 0) {
		syslog(LOG_ERR, "codb_open: mmit after open %s", mdb_strerror(rc));
		mdb_env_close(db->env);
		free(db);
		return (NULL);
	}

	return (db);
}

void db_close(struct db *db)
{
	if (db == NULL)
		return;

	mdb_env_close(db->env);
	free(db);
}

int db_del(struct db *db, char *keystr)
{
	int rc;
	MDB_val key;
	MDB_txn *txn;

	rc = mdb_txn_begin(db->env, NULL, 0, &txn);
	if (rc != 0) {
		syslog(LOG_ERR, "db_del: mdb_txn_begin: %s", mdb_strerror(rc));
		return (rc);
	}

	key.mv_data	= keystr;
	key.mv_size	= strlen(keystr);

	rc = mdb_del(txn, db->dbi, &key, NULL);
	if (rc != 0 && rc != MDB_NOTFOUND) {
		syslog(LOG_ERR, "db_del: mdb_del: %s", mdb_strerror(rc));
		/* fall through to commit */
	}

	rc = mdb_txn_commit(txn);
	if (rc) {
		syslog(LOG_ERR, "db_del: mdb_txn_commit: (%d) %s", rc, mdb_strerror(rc));
		mdb_txn_abort(txn);
	}
	return (rc);
}

/* Return 0 if record can be stored */
int db_put(struct db *db, char *keystr, char *payload)
{
	int rc;
	MDB_val key, data;
	MDB_txn *txn;

	assert(db != NULL);

	if (strcmp(payload, "DELETE") == 0)
		return db_del(db, keystr);

	rc = mdb_txn_begin(db->env, NULL, 0, &txn);
	if (rc != 0) {
		syslog(LOG_ERR, "db_put: mdb_txn_begin: %s", mdb_strerror(rc));
		return (-1);
	}

	key.mv_data	= keystr;
	key.mv_size	= strlen(keystr);
	data.mv_data	= payload;
	data.mv_size	= strlen(payload) + 1;	/* including nul-byte so we can
						 * later decode string directly
						 * from this buffer */

	rc = mdb_put(txn, db->dbi, &key, &data, 0);
	if (rc != 0) {
		syslog(LOG_ERR, "db_put: mdb_put: %s", mdb_strerror(rc));
		/* fall through to commit */
	}

	rc = mdb_txn_commit(txn);
	if (rc) {
		syslog(LOG_ERR, "db_put: mdb_txn_commit: (%d) %s", rc, mdb_strerror(rc));
		mdb_txn_abort(txn);
	}
	return (rc);
}

long db_get(struct db *db, char *k, char *buf, long buflen)
{
	MDB_val key, data;
	MDB_txn *txn;
	int rc;
	long len = -1;

	if (db == NULL)
		return (-1);

	rc = mdb_txn_begin(db->env, NULL, MDB_RDONLY, &txn);
	if (rc) {
		syslog(LOG_ERR, "db_get: mdb_txn_begin: (%d) %s", rc, mdb_strerror(rc));
		return (-1);
	}

	key.mv_data = k;
	key.mv_size = strlen(k);

	rc = mdb_get(txn, db->dbi, &key, &data);
	if (rc != 0) {
		if (rc != MDB_NOTFOUND) {
			printf("get: %s\n", mdb_strerror(rc));
		} else {
			// printf(" [%s] not found\n", k);
		}
	} else {
		len =  (data.mv_size < buflen) ? data.mv_size : buflen;
		memcpy(buf, data.mv_data, len);
		// printf("%s\n", (char *)data.mv_data);
	}
	mdb_txn_commit(txn);
	return (len);
}

void db_dump(char *path, char *lmdbname)
{
	struct db *db;
	MDB_val key, data;
	MDB_txn *txn;
	MDB_cursor *cursor;
	int rc;

	if ((db = db_open(path, lmdbname, true)) == NULL) {
		fprintf(stderr, "Cannot open lmdb/%s at %s\n",
			lmdbname ? lmdbname : "NULL", path);
		return;
	}

	key.mv_size = 0;
	key.mv_data = NULL;

	rc = mdb_txn_begin(db->env, NULL, MDB_RDONLY, &txn);
	if (rc) {
		syslog(LOG_ERR, "db_dump: mdb_txn_begin: (%d) %s", rc, mdb_strerror(rc));
		db_close(db);
		return;
	}

	rc = mdb_cursor_open(txn, db->dbi, &cursor);

	/* -1 because we 0-terminate strings */
	while ((rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT)) == 0) {
		printf("%*.*s %*.*s\n",
			(int)key.mv_size, (int)key.mv_size, (char *)key.mv_data,
			(int)data.mv_size - 1, (int)data.mv_size - 1, (char *)data.mv_data);
	}
	mdb_cursor_close(cursor);
	mdb_txn_commit(txn);
	db_close(db);
}

void db_list(char *path, char *lmdbname, int (*func)(int keylen, char *key, int datlen, char *data))
{
	struct db *db;
	MDB_val key, data;
	MDB_txn *txn;
	MDB_cursor *cursor;
	int rc, stop;

	if ((db = db_open(path, lmdbname, true)) == NULL) {
		fprintf(stderr, "Cannot open lmdb/%s at %s\n",
			lmdbname ? lmdbname : "NULL", path);
		return;
	}

	key.mv_size = 0;
	key.mv_data = NULL;

	rc = mdb_txn_begin(db->env, NULL, MDB_RDONLY, &txn);
	if (rc) {
		syslog(LOG_ERR, "db_dump: mdb_txn_begin: (%d) %s", rc, mdb_strerror(rc));
		db_close(db);
		return;
	}

	rc = mdb_cursor_open(txn, db->dbi, &cursor);

	while ((rc = mdb_cursor_get(cursor, &key, &data, MDB_NEXT)) == 0) {
		stop = func(key.mv_size, key.mv_data, data.mv_size, data.mv_data);
		if (stop == 1) {
			break;
		}
	}
	mdb_cursor_close(cursor);
	mdb_txn_commit(txn);
	db_close(db);
}

void db_load(char *path, char *lmdbname)
{
	struct db *db;
	char buf[8192], *bp;

	if ((db = db_open(path, lmdbname, false)) == NULL) {
		syslog(LOG_ERR, "db_load: db_open");
		return;
	}

	while (fgets(buf, sizeof(buf), stdin) != NULL) {

		if ((bp = strchr(buf, '\r')) != NULL)
			*bp = 0;
		if ((bp = strchr(buf, '\n')) != NULL)
			*bp = 0;

		if ((bp = strchr(buf, ' ')) != NULL) {
			*bp = 0;

			if (db_put(db, buf, bp+1) != 0) {
				fprintf(stderr, "Cannot load key\n");
			}
		}
	}

	db_close(db);
}

/*
 * Return number of entries in database at `db'
 */

size_t db_numentries(struct db *db)
{
	MDB_stat st;

	assert(db != NULL);


	if (mdb_env_stat(db->env, &st) != 0)
		return -1L;

	return (st.ms_entries);
}

/*
 * Return pointer to database path; do not touch, do not free!
 */

const char *db_getpath(struct db *db)
{
	const char *path;

	assert(db != NULL);
	if (mdb_env_get_path(db->env, &path) != 0)
		return (NULL);
	return (path);
}

#ifdef TESTING

#define DBNAME "t/"

int main(int argc, char **argv)
{
	struct db *db;
	char buf[BUFSIZ];
	int rc;

	if ((db = db_open(DBNAME, NULL, false)) == NULL) {
		perror(DBNAME);
		return (1);
	}

	if (db_get(db, "jp", buf, sizeof(buf)) > 0) {
		printf("%s\n", buf);
	} else {
		rc = db_put(db, "jp", "This is my name");
		printf("put %d\n", rc);
	}

	db_close(db);

	return (0);
}
#endif
