import requests
import json

__author__    = 'Jan-Piet Mens <jp@mens.de>'
__copyright__ = 'Copyright 2018 Jan-Piet Mens'
__license__   = """GNU General Public License"""

class RevgeodGeocode(object):

    """
    Geocoder object. Initialize with host, port

        >>> geocoder = RevgeodGeocode(host="127.0.0.1", port=8865)

    Reverse geocode a latitude & longitude into an address dict:

        >>> geocoder.reverse_geocode(51.5104, -0.1021)

    """

    def __init__(self, host="127.0.0.1", port=8865):

        self.url = "http://{host}:{port}/rev".format(host=host, port=port)
        self.session = requests.Session()
        self.session.headers.update({
                "Content-type"  : "application/json"
        })

    def reverse_geocode(self, lat, lon):
        """
        Given a lat, lon, return the address for that point from revgeod
        """
        params = {
            u'lat' : float_it(lat),
            u'lon' : float_it(lon),
        }
        data = None

        r = self.session.get(self.url, params=params)
        try:
            data = json.loads(r.content)
            if 'address' in data:
                data = data['address']
        except Exception as e:
            print(str(e))

        data.update(**params)
        return data

def float_it(float_string):
    try:
        return float(float_string)
    except ValueError:
        return float_string
