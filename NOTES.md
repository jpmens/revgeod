
## traccar

We use `revgeod` extensively with [Traccar](https://www.traccar.org) and configure the latter with:

```xml
<entry key='geocoder.enable'>true</entry>
<entry key='geocoder.type'>nominatim</entry>
<entry key='geocoder.url'>http://127.0.0.1:8865/rev</entry>
<entry key='geocoder.onRequest'>true</entry>
<entry key='geocoder.format'>%t</entry>
```

The `%t` format is one of Traccar's available parameters, currently defined as

```
/**
 * Available parameters:
 *
 * %p - postcode
 * %c - country
 * %s - state
 * %d - district
 * %t - settlement (town)
 * %u - suburb
 * %r - street (road)
 * %h - house
 * %f - formatted address
 *
```

so internally [they use that value](https://github.com/traccar/traccar/tree/master/src/main/java/org/traccar/geocoder) as:

```java
result = replace(result, "%t", address.getSettlement());
```

in the [nominatim geocoder](https://github.com/traccar/traccar/blob/master/src/main/java/org/traccar/geocoder/NominatimGeocoder.java).

The `village` element returned by `revgeod` is used therein like:

```java
if (result.containsKey("village")) {
    address.setSettlement(result.getString("village"));
} else if (result.containsKey("town")) {
    address.setSettlement(result.getString("town"));
} else if (result.containsKey("city")) {
    address.setSettlement(result.getString("city"));
}
```
