import time
import json
import paho.mqtt.client as mqtt

# Configuration
BROKER = "10.42.0.1"   # e.g. "test.mosquitto.org"
PORT = 1883
TOPIC = "pedal"
CLIENT_ID = "publisher_001"

# Callback when connected
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to broker")
    else:
        print(f"Connection failed with code {rc}")

# Callback when message is published
def on_publish(client, userdata, mid):
    print(f"Message {mid} published")

# Create client
client = mqtt.Client(client_id=CLIENT_ID)

client.on_connect = on_connect
client.on_publish = on_publish

# Connect to broker
client.connect(BROKER, PORT, keepalive=60)

# Start network loop
client.loop_start()

pedal_num = 0



try:
    while True:

        pedal_num += 1

        if (pedal_num == 20): 
                pedal_num = 0

        payload = {
            "timestamp": time.time(),
            "accelerator": pedal_num
        }

        result = client.publish(TOPIC, json.dumps(payload), qos=1)

        #result = client.publish(TOPIC, f"{pedal_num}", qos=1)

        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            print(f"Sent --{pedal_num}--to topic `{TOPIC}`")
        else:
            print(f"Failed to send message to topic `{TOPIC}`")

        time.sleep(0.25)

except KeyboardInterrupt:
    print("Stopping publisher...")

finally:
    client.loop_stop()
    client.disconnect()
