import paho.mqtt.client as mqtt

from time import sleep

def on_connect(client, userdata, flags, rc):
	print("Connected with result code: " + str(rc))
	

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect("localhost", 1883, 60)
client.loop_forever()
