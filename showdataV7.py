import time

t0 = time.time()
import sys
import os.path
import math
t1 = time.time()
print(t1-t0)

USER_API_KEY = ''
CHANNEL_ID = ''
WRITE_API_KEY = ''

found = False

file_path = os.path.join("/root", "id.txt")
temperature = float(sys.argv[1])
percentHumidity = float(sys.argv[2])
pressure = float(sys.argv[3])
latitud = float(sys.argv[4])
longitud = float(sys.argv[5])
vbatt = float(sys.argv[6])
ibatt = float(sys.argv[7])

def computeHeatIndex():
    temperature_F = temperature * 1.8 + 32;

    hi = 0.5 * (temperature_F + 61.0 + ((temperature_F - 68.0) * 1.2) + (percentHumidity * 0.094))

    if(hi > 79):
        hi = -42.379 + 2.04901523 * temperature_F + 10.14333127 * percentHumidity + -0.22475541 * temperature_F*percentHumidity + -0.00683783 * pow(temperature_F, 2) + -0.05481717 * pow(percentHumidity, 2) + 0.00122874 * pow(temperature_F, 2) * percentHumidity + 0.00085282 * temperature_F*pow(percentHumidity, 2) + -0.00000199 * pow(temperature_F, 2) * pow(percentHumidity, 2)

    if((percentHumidity < 13.0) and (temperature_F >= 80.0) and (temperature_F <= 112.0)):
        hi -= ((13.0 - percentHumidity) * 0.25) * sqrt((17.0 - abs(temperature_F - 95.0)) * 0.05882)
    elif((percentHumidity > 85.0) and (temperature_F >= 80.0) and (temperature_F <= 87.0)):
        hi += ((percentHumidity - 85.0) * 0.1) * ((87.0 - temperature_F) * 0.2)

    hi = (hi - 32) * 0.55555;

    result = float("{0:.2f}".format(hi))
    return result
"""
def dewpoint_approximation():
    dewPoint = (237.7 * gamma()) / (17.271 - gamma())
    
    result = float("{0:.2f}".format(dewPoint))
    return result

def gamma():
    gamma = (17.271 * temperature / (237.7 + temperature)) + math.log(percentHumidity / 100.0)
    return gamma
"""
def createFile(id, write_api_key):
    file = open(file_path, 'w')
    file.write(id)
    file.write("\r\n") 
    file.write(write_api_key)
    file.close()

if(os.path.exists(file_path)):
    print('Existe ' + file_path)

    file = open(file_path, 'r')
    lines = file.read().splitlines()
    CHANNEL_ID = lines[0]
    WRITE_API_KEY = lines[1]

    found = True

    """
    import requests
    import json
    
    uri = 'https://api.thingspeak.com/channels/' + id + '.json'
    payload = {'api_key': USER_API_KEY}

    print("HTTP request for getting channel info...")
    print(uri)
    
    t3 = time.time()
    r = requests.get(uri, params=payload)
    print("Status: " + str(r.status_code))
    t5 = time.time()
    print(t5-t3)

    data = r.json()
    if(data['id'] == int(id)):
        WRITE_API_KEY = str(data['api_keys'][0]['api_key'])
        print("Channel found: " + CHANNEL_ID)
        found = True
    """

else:
    print('No existe ' + file_path)

    import requests
    import json

    uri = 'https://api.thingspeak.com/channels.json'
    payload = {'api_key': USER_API_KEY,
               'name': "Mobile weather station",
               'description': 'Faculty of Engineering in Bilbao, University of the Basque Country (UPV/EHU)',
               'field1': 'TEMPERATURE',
               'field2': 'HUMIDITY',
               'field3': 'HEAT INDEX',
               'field4': 'ATMOSPHERIC PRESSURE',
               'field5': 'Vbatt',
			   'field6': 'Ibatt',
               'field7': 'LATITUDE',
               'field8': 'LONGITUDE',
               'public_flag': 'true',
               'tags': 'PiE'}

    print("HTTP request for creating the channel...")
    print(uri)

    r = requests.post(uri, params=payload)
    print("STATUS: " + str(r.status_code))

    data = r.json()
    CHANNEL_ID = str(data['id'])
    WRITE_API_KEY = str(data['api_keys'][0]['api_key'])
    createFile(str(data['id']), WRITE_API_KEY)
    print("Channel created: " + CHANNEL_ID)

    found = True

def doit():
    heatIndex = computeHeatIndex()
    #dewPoint = dewpoint_approximation()

    import paho.mqtt.publish as publish

    mqttHost = "mqtt.thingspeak.com"
    topic = "channels/" + CHANNEL_ID + "/publish/" + WRITE_API_KEY
    payload = "field1=" + str(temperature) + "&field2=" + str(percentHumidity) + "&field3=" + str(heatIndex) + "&field4=" + str(pressure) + "&field5=" + str(vbatt) + "&field6=" + str(ibatt) + "&field7=" + str(latitud) + "&field8=" + str(longitud)

    print("Updating the channel feed...")
    print(topic)
    
    t7 = time.time()
    try:
        publish.single(topic, payload, hostname=mqttHost)
    except:
        print("There was an error while publishing the data to MQTT broker.")
    t13 = time.time()
    print(t13-t7)

if(found):
    doit()
else:
    print("Channel not found")
