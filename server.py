import paho.mqtt.client as mqtt
import time

def on_connect(client, userdata, flags, rc):
	print("Connected with result code: " + str(rc))
	client.subscribe("sensors/#")
	client.subscribe("telegram/#")

def on_message(client, userdata, message):
	topic = message.topic
	data = message.payload
	timestamp = time.time()

	if topic.startswith("sensors/audio"):
		save_audio_file(topic, data)
	
	print("Received message")

def save_audio_file(topic, data, timestamp):
	filename = topic.split("/")[-1] + str(timestamp)
	filepath = "./audio/" + filename
	with open(filepath, "wb") as audio_file:
		audio_file.write(data)

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect("localhost", 1883, 60)
client.loop_forever()
