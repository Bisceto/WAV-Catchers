import paho.mqtt.client as mqtt

def on_connect(client, userdata, flags, rc):
    print("Connected with result code: " + str(rc))
    client.subscribe("motion")

def on_message(client, userdata, message):
    if message.topic == "motion":
        proximity_state = message.payload.decode()
        print(f"Proximity Sensor State: {proximity_state}")

        if proximity_state == "object detected":
            client.publish("classification", "Take photo")
        elif proximity_state == "no object":
            client.publish("classification", "Don't take photo")

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect("localhost", 1883, 60)
client.loop_forever()
