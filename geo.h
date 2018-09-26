#include "json.h"

void revgeo_init();
int revgeo_getdata(char *apikey, double lat, double lon, UT_string *addr, UT_string *rawdata);
void revgeo_free();

#define UB(x)   utstring_body(x)
