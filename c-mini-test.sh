#!/bin/sh

# {"address": {"village": "4 Rue du G\u00e9n\u00e9ral Lambert, 75015 Paris, France"}}
# 4 Rue du Général Lambert, 75015 Paris, France
lat=48.85593
lon=2.29431
url="http://127.0.0.1:8865/rev?lat=${lat}&lon=${lon}"

json=$(curl  -sS $url)

str=$(echo "$json" | jq -r .address.village)

printf "%s\n%s\n", "$json", "$str"


