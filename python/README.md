## Python _revgeod_

This is a small Python class (tested with Python2, Python3) which will query [revgeod](https://github.com/jpmens/revgeod).


### Ansible

During a training at the end of December, I was talked into rewriting an example lookup plugin for a training class because the example I had was unrealistic. So I did so and used _revgeo_ as an example.

The Ansible playbook I use for testing the plugin:

```yaml
- hosts: localhost
  vars:
    - location: "{{ lookup('geo', '50.45464,7.86665') }}"
  tasks:
  - debug: var=location

  - debug: msg="Jane is now at {{ lookup('geo', '48.85833,3.29513') }}"
```

```
TASK [debug] *******************************************************************
ok: [localhost] => {
    "location": "Aral Heiligenroth, A 3, 56412 Heiligenroth, Germany"
}

TASK [debug] *******************************************************************
ok: [localhost] => {
    "msg": "Jane is now at La Terre Noire, 77510 Sablonnières, France"
}
```

and the Ansible lookup plugin proper (no error handling, no options; just demo):

```python
#!/usr/bin/python
# -*- coding: utf-8 -*-
# (c)2018, Jan-Piet Mens <jp@mens.de>

from ansible.plugins.lookup import LookupBase
from revgeod.geocoder import RevgeodGeocode

class LookupModule(LookupBase):
    def run(self, terms, variables, **kwargs):

        geocoder = RevgeodGeocode(host="127.0.0.1", port=8865)
        ret = []
        for term in terms:
            lat,lon =  term.split(',')
            a = geocoder.reverse_geocode(lat, lon)

            ret.append(a['village'])
        return ret
```
