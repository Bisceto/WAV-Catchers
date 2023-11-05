import paho.mqtt.client as mqtt
import time

global password_audio_filename

password_audio_filename : str = 'recording.wav'

def on_connect(client, userdata, flags, rc):
    print("Connected with result code: " + str(rc))
    client.subscribe("sensors/#")
    client.subscribe("telegram/#")

def on_message(client, userdata, message):
    topic = message.topic
    data = message.payload
    timestamp = time.time()
    
    if topic.startswith("sensors/microphone/snippet"):
        save_audio_snippet(message.payload)
        print("Audio message received of length", len(message.payload))
    if topic.startswith("sensors/microphone/recording_finished"):
        #is_microphone_recording = False
        print("Microphone recording finished")
    if topic.startswith("sensors/microphone/recording_started"):
        #is_microphone_recording = True
        clear_recording()
        print("Microphone has started recording")


def clear_recording():
    open(password_audio_filename, 'w').close()

def save_audio_snippet(message_payload : str):
    f = open(password_audio_filename, 'ab')
    f.write(message_payload)
    f.close()

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
