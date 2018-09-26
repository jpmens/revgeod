#ifndef _DB_H_INCLUDED_
# define _DB_H_INCLUDED_

#include <lmdb.h>

// #define LMDB_DB_SIZE	((size_t)100000 * (size_t)(1024 * 1024))
#define LMDB_DB_SIZE    ((size_t)10240 * (size_t)(1024 * 1024))

struct db {
	MDB_env *env;
	MDB_dbi dbi;
};

struct db *db_open(char *path, char *dbname, int rdonly);
void db_close(struct db *);
int db_put(struct db *, char *ghash, char *payload);
long db_get(struct db *, char *key, char *buf, long buflen);
void db_dump(char *path, char *lmdbname);
void db_load(char *path, char *lmdbname);

#endif
