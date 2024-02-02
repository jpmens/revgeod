#!/bin/sh

# {"address": {"village": "4 Rue du G\u00e9n\u00e9ral Lambert, 75015 Paris, France"}}
# 4 Rue du Général Lambert, 75015 Paris, France
lat=48.85593
lon=2.29431

host=${REVGEO_HOST:="127.0.0.1"}
port=${REVGEO_PORT:=8865}

url="http://$host:$port/rev?lat=${lat}&lon=${lon}&app=c-mini-test"

echo $url

json=$(curl -sSf $url)

str=$(echo "$json" | jq -r .address.village)

printf "%s\n%s\n", "$json", "$str"


