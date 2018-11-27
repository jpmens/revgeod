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

#include "json.h"

void revgeo_init();
int revgeo_getdata(char *apikey, double lat, double lon, UT_string *addr, UT_string *rawdata, UT_string *locality, UT_string *cc);
void revgeo_free();
bool http_get(char *url, UT_string *curl_buf);

#define UB(x)   utstring_body(x)
