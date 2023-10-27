import paho.mqtt.client as mqtt
import time
import cv2
import cvlib as cv
from cvlib.object_detection import draw_bbox
import paho.mqtt.client as mqtt
from time import sleep

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



def on_connect(client, userdata, flags, rc):
    print("Connected with result code: " + str(rc))
    print("Waiting for 2 seconds")
    sleep(2)
    print("Sending message.")
    client.publish("detection/camera", "Person detected")


video = cv2.VideoCapture(0)

labels = []

while True:
    ret, frame = video.read()
    bbox, label, conf = cv.detect_common_objects(frame, model="yolov3")
    output_image = draw_bbox(frame, bbox, label, conf)

    for item in label:
        if item in labels:
            pass
        else:
            labels.append(item)

    cv2.imshow("At your door", output_image)

    if cv2.waitKey(1) & 0xFF == ord("q"):
        cv2.destroyWindow("At your door")
        break

print(labels)
if "person" in labels:
    client = mqtt.Client()
    client.on_connect = on_connect
    client.connect("localhost", 1883, 60)
    client.loop_forever() #starts listening
