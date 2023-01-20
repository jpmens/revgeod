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
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>
#include <curl/curl.h>
#include "utstring.h"
#include "geo.h"
#include "json.h"

#define OPENCAGE_URL "https://api.opencagedata.com/geocode/v1/json?q=%lf+%lf&key=%s&abbrv=1&no_record=1&limit=1&format=json"
#define LOCATIONIQ_URL "https://eu1.locationiq.com/v1/reverse?key=%s&lat=%lf&lon=%lf&normalize_address=1&normalizecity=1&format=json"

static CURL *curl;

static size_t writemem(void *contents, size_t size, size_t nmemb, void *userp)
{
	UT_string *curl_buf = (UT_string *)userp;
	size_t realsize = size * nmemb;

	utstring_bincpy(curl_buf, contents, realsize);

	return (realsize);
}

/*
 * Decode the JSON string we got from Locationiq, which is now in `geodata'
 * results_array is not filled
 */

static bool locationiq_decode(UT_string *geodata, UT_string *addr, UT_string *results_array, UT_string *locality, UT_string *cc)
{
	JsonNode *json, *display_name, *address;

	/*
	 * https://eu1.locationiq.com/v1/reverse?key=<Your_API_Access_Token>&lat=50.880067&lon=4.6695490&format=json
	 *
	 * {
	 * "place_id": "118683862",
	 * "licence": "https://locationiq.com/attribution",
	 * "osm_type": "way",
	 * "osm_id": "42772415",
	 * "lat": "50.880016600000005",
	 * "lon": "4.670175326848774",
	 * "display_name": "Donorcentrum Leuven, 49, Ring Noord, Leuven, Flemish Brabant, Flanders, 3000, Belgium",
	 * "address": {
	 *	"address29": "Donorcentrum Leuven",
	 *	"house_number": "49",
	 *  "road": "Ring Noord",
	 *	"city_district": "Leuven",
	 *	"city": "Leuven",
	 *	"county": "Leuven",
	 *	"state": "Flemish Brabant",
	 *	"region": "Flanders",
	 *	"postcode": "3000",
	 *	"country": "Belgium",
	 *	"country_code": "be"
	 * },
	 * "boundingbox": [
	 *	  "50.8797397",
	 *	  "50.8802218",
	 *	  "4.6697093",
	 *	  "4.6706693"
	 *   ]
	 * }
	 */

	utstring_clear(results_array);

	if ((json = json_decode(UB(geodata))) == NULL) {
		fprintf(stderr, "Cannot decode JSON from %s\n", UB(geodata));
		return (false);
	}

	// get addr from display_name 
	display_name = json_find_member(json, "display_name");
	if ((display_name != NULL) && (display_name->tag == JSON_STRING)) {
		utstring_printf(addr, "%s", display_name->string_);
	}

	
	if ((address = json_find_member(json, "address")) != NULL) {
		JsonNode *j;

		// get cc from 'country_code'
		// and capitalize
		if ((j = json_find_member(address, "country_code")) != NULL) {
			if (j->tag == JSON_STRING) {
				char *bp = j->string_;
				int ch;

				while (*bp) {
					ch = (islower(*bp)) ? toupper(*bp) : *bp;
					utstring_printf(cc, "%c", ch);
					++bp;
				}
			}
		}

		// get locality from 'city'
		if ((j = json_find_member(address, "city")) != NULL) {
			if (j->tag == JSON_STRING) {
				utstring_printf(locality, "%s", j->string_);
			}
		}
	}

	json_delete(json);
	return (true);
}

/*
 * Decode the JSON string we got from OpenCage, which is now in `geodata'
 * results_array becomes the JSON string of results[]
 */

static bool opencage_decode(UT_string *geodata, UT_string *addr, UT_string *results_array, UT_string *locality, UT_string *cc)
{
	JsonNode *json, *results, *address, *zeroth, *ac;

	/*
	* We are parsing this. I want the formatted in `addr'
	*
	* {
	*   "documentation": "https://geocoder.opencagedata.com/api",
	*   "licenses": [
	*     {
	*       "name": "CC-BY-SA",
	*       "url": "http://creativecommons.org/licenses/by-sa/3.0/"
	*     },
	*     {
	*       "name": "ODbL",
	*       "url": "http://opendatacommons.org/licenses/odbl/summary/"
	*     }
	*   ],
	*   "rate": {
	*     "limit": 2500,
	*     "remaining": 2495,
	*     "reset": 1525392000
	*   },
	*   "results": [
	*     {
	*       ...
	*       "components": {
	*         "city": "Sablonnières",
	*         "country": "France",
	*         "country_code": "fr",
	*         "place": "La Terre Noire",
	*       },
	*       "formatted": "La Terre Noire, 77510 Sablonnières, France",
	*/

	utstring_clear(results_array);

	if ((json = json_decode(UB(geodata))) == NULL) {
		fprintf(stderr, "Cannot decode JSON from %s\n", UB(geodata));
		return (false);
	}

	if ((results = json_find_member(json, "results")) != NULL) {
		char *js = json_stringify(results, NULL);

		if (js != NULL) {
			utstring_printf(results_array, "%s", js);
			free(js);
		}
		if ((zeroth = json_find_element(results, 0)) != NULL) {
			address = json_find_member(zeroth, "formatted");
			if ((address != NULL) && (address->tag == JSON_STRING)) {
				utstring_printf(addr, "%s", address->string_);
			}
		}

		if ((ac = json_find_member(zeroth, "components")) != NULL) {

			/*
			 * {
			 *   "ISO_3166-1_alpha-2": "FR",
			 *   "_type": "place",
			 *   "city": "Sablonnières",
			 *   "country": "France",
			 *   "country_code": "fr",
			 *   "county": "Seine-et-Marne",
			 *   "place": "La Terre Noire",
			 *   "political_union": "European Union",
			 *   "postcode": "77510",
			 *   "state": "Île-de-France"
			 * }
			 */

			JsonNode *j;

			if ((j = json_find_member(ac, "country_code")) != NULL) {
				if (j->tag == JSON_STRING) {
					char *bp = j->string_;
					int ch;

					while (*bp) {
						ch = (islower(*bp)) ? toupper(*bp) : *bp;
						utstring_printf(cc, "%c", ch);
						++bp;
					}
				}
			}

			if ((j = json_find_member(ac, "city")) != NULL) {
				if (j->tag == JSON_STRING) {
					utstring_printf(locality, "%s", j->string_);
				}
			}
		}

	}

	json_delete(json);
	return (true);
}

bool http_get(char *url, UT_string *curl_buf)
{
	long http_code;
	CURLcode res;

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 2000L);
	curl_easy_setopt(curl, CURLOPT_VERBOSE, 0);

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writemem);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)curl_buf);

	# DEBUG: see actual URL that is being called
	#fprintf(stderr, "http_get(%s)\n", url);


	res = curl_easy_perform(curl);
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

	if (res != CURLE_OK || http_code != 200) {
		fprintf(stderr, "curl_easy_perform() failed: %s: HTTP status code == %ld\n",
		              curl_easy_strerror(res), http_code);
		return (false);
	}
	return (true);
}

/*
 * apikey: API key for OpenCage
 * lat, lon: 
 * addr: target for the parsed address
 * rawdata: an allocated UT_string which is cleared internally; will contain response from OpenCage
 * return true if OK, false otherwise
 */

int revgeo_getdata(char *apikey, char *api_provider, double lat, double lon, UT_string *addr, UT_string *rawdata, UT_string *locality, UT_string *cc)
{
	static UT_string *url;
	static UT_string *curl_buf;
	int rc;
	bool bf;

	if (lat == 0.0L && lon == 0.0L) {
		utstring_printf(addr, "Unknown (%lf,%lf)", lat, lon);
		return (false);
	}
	
	utstring_renew(url);
	utstring_renew(curl_buf);
	utstring_clear(rawdata);

	if (strcmp(api_provider,"opencage") == 0) {
		utstring_printf(url, OPENCAGE_URL, lat, lon, apikey);

		bf = http_get(UB(url), curl_buf);
		if (bf == true) {
			if ((rc = opencage_decode(curl_buf, addr, rawdata, locality, cc)) == false) {
				bf = false;
			}
		}
	}else{
		utstring_printf(url, LOCATIONIQ_URL, apikey, lat, lon );

		bf = http_get(UB(url), curl_buf);
		if (bf == true) {
			if ((rc = locationiq_decode(curl_buf, addr, rawdata, locality, cc)) == false) {
				bf = false;
			}
		}
	}

	return (bf);
}

void revgeo_init()
{
	curl = curl_easy_init();
}

void revgeo_free()
{
	curl_easy_cleanup(curl);
	curl = NULL;
}

#if TESTING
int main()
{
	double lat = 49.0156556, lon = 8.3975169;
	double clat = 48.85833, clon = 3.29513;
	static UT_string *addr, *rawdata, *locality, *cc;

	char *apikey = getenv("OPENCAGE_APIKEY");

	utstring_renew(addr);
	utstring_renew(rawdata);
	utstring_renew(locality);
	utstring_renew(cc);


	if (apikey == NULL) {
		fprintf(stderr, "OPENCAGE_APIKEY key missing in environment\n");
		exit(2);
	}

	revgeo_init();

	if (revgeo_getdata(apikey, lat, lon, addr, rawdata, locality, cc)) {
		printf("%s\n", UB(addr));
	} else {
		printf("Cannot get revgeo\n");
	}

	utstring_renew(addr);

	if (revgeo_getdata(apikey, clat, clon, addr, rawdata, locality, cc)) {
		printf("%s\n", UB(addr));
		{
			FILE *fp = fopen("1.1", "w");
			fprintf(fp, "%s\n", UB(rawdata));
			fclose(fp);
		}
	} else {
		printf("Cannot get revgeo\n");
	}

	revgeo_free();

	return (0);

}
#endif
