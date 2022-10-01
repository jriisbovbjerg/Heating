# Fetches data from https://www.energidataservice.dk/tso-electricity/elspotprices
# API documentation http://docs.ckan.org/en/latest/api/index.html#making-an-api-request

import datetime
import random
import time
import requests

from influxdb import InfluxDBClient

INFLUXDB_ADDRESS = '192.168.10.7'
INFLUXDB_PORT = 8086
INFLUXDB_USER = 'admin'
INFLUXDB_PASSWORD = 'admin'
INFLUXDB_DATABASE = 'statsdemo'


"""
    DK_WEST = West Denmark (zip-code above 4999)
    DK_EAST = East Denmark (zip-code below 5000)
"""
DK_WEST = "DK1"
DK_EAST = "DK2"

def createHeader():
    apiHeader = {
        "Content-Type": "application/json"
    }

    return [apiHeader]


def dateFormat(date):
    return date.strftime("%Y-%m-%dT%H:%M:%SZ")


def format_timestamp(timestring):
    #print (timestring)
    return 1_000_000_000 * int(time.mktime(datetime.datetime.strptime(timestring, "%Y-%m-%dT%H:%M:%S").timetuple()))

def spotprices(pricezone):
        """
        Gives the spot prices in the specified from/to range.
        Spot prices are delivered ahead of our current time, up to 24 hours ahead.
        The price is excluding VAT and tarrifs.
        """
        try:
            headers = createHeader()
            url = "https://api.energidataservice.dk/datastore_search?resource_id=elspotprices&limit=48&filters={\"PriceArea\":\"DK1\"}&sort=HourUTC desc"
            return requests.get(url, headers=headers[0]).json()

        except Exception as ex:
            raise Exception(str(ex))

def send_sensor_data_to_influxdb(influxdb_client, time, value):
    timestamp_start = format_timestamp(time) + random.randint(1,1000)
    timestamp_end = format_timestamp(time) - random.randint(1,1000) + 3_600_000_000_000
    json_body = [
        {
            'measurement': "spotprice",
            'time' : timestamp_start,
            'tags': {
                'market': DK_WEST
            },
            'fields': {
                'value': value,
            }
        },{
            'measurement': "spotprice",
            'time' : timestamp_end,
            'tags': {
                'market': DK_WEST
            },
            'fields': {
                'value': value,
            }
        }

    ]
    print(json_body)
    influxdb_client.write_points(json_body)

def filter_current_price(result):
    hour_dk = result["HourDK"]
    current_hour = time.strftime("%Y-%m-%dT%H:00:00")
    return hour_dk == current_hour

influxdb_client = InfluxDBClient(INFLUXDB_ADDRESS, INFLUXDB_PORT, INFLUXDB_USER, INFLUXDB_PASSWORD, INFLUXDB_DATABASE)

try:
    response = spotprices(DK_WEST)
    prices = response["result"]["records"]
    
    if len(prices) == 0:
        raise Exception("No current hour result")
    
    for price in prices:
            kwh_price = float(price["SpotPriceEUR"]) * 7.46 / 1000.0
            hour = price["HourDK"]

            send_sensor_data_to_influxdb(influxdb_client, hour, kwh_price)

except Exception as inst:
    print("Exception: " + inst)

# To ensure that the message is actually published (async)
time.sleep(5)