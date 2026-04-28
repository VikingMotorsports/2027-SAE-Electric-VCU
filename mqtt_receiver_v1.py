"""
FSAE MQTT Receiver v1.0
Receives telemetry data from Raspberry Pi via WiFi/MQTT and displays it in real time.

Uses paho-mqtt library: pip install paho-mqtt
Connected to Louis's MQTT WiFi network

SETUP:
1. Update CONFIGURATION section below with Louis's details
2. Connect laptop to Louis's WiFi network
3. Run: python mqtt_receiver_v1.py
"""

import paho.mqtt.client as mqtt
import json
from datetime import datetime 

# CONFIGURATION - Update these values!

# Louis's Raspberry Pi Settings
PI_IP_ADDRESS = "10.42.0.1"       # TODO: get from Louis
MQTT_PORT = 1883                      # Standard MQTT port
MQTT_TOPIC = "test"                 # TODO: Confirm with Louis

# Global Variables

message_count = 0
session_start_time = None

# MQTT Event Handlers

def on_connect(client, userdata, flags, rc):
  
    global session_start_time
    
    print("\n" + "="*70)
    if rc == 0:                         # rc (int): result code 0 = success
        print("CONNECTION SUCCESSFUL")
        print(f"  Broker: {PI_IP_ADDRESS}:{MQTT_PORT}")
        print(f"  Topic: {MQTT_TOPIC}")
        print("="*70)
        
        # Subscribe to telemetry topic
        client.subscribe(MQTT_TOPIC)
        print("SUBSCRIBED to MQTT TOPIC")
        print("\nWaiting for messages... (Press Ctrl+C to stop)\n")
        
        session_start_time = datetime.now()
    else:
        print("CONNECTION FAILED")
        print(f"  Result code: {rc}")
        print("="*70)

# Triggered when message is received
def on_message(client, userdata, msg):  
    
    global message_count
    message_count += 1
    
    # Get timestamp
    timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
    
    # Decode message
    payload = msg.payload.decode('utf-8')
    
    # Display message
    print("─" * 70)
    print(f"[{message_count:05d}] {timestamp} | Topic: {msg.topic}")
    print("─" * 70)
    
    try:
        # Try parsing as JSON
        data = json.loads(payload)
        
        # Display as formatted JSON
        print("Telemetry Data:")
        for key, value in data.items():
            # Format numeric values
            if isinstance(value, float):
                print(f"  {key:25s} = {value:.2f}")
            else:
                print(f"  {key:25s} = {value}")
    except json.JSONDecodeError:
        # Not JSON - display as plain text
        print(f"Message: {payload}")
    
    print("─" * 70)
    print()

# Triggered when disconnected
def on_disconnect(client, userdata, rc):

    if rc != 0:
        print("\nUnexpected disconnection")


#--------------------------------------------------------------------
# Main Program
#--------------------------------------------------------------------

def main():
    
    # Display header
    print("\n" + "="*70)
    print("  MQTT RECEIVER")
    print("  Team 13")
    print("="*70)
    print("\nConfiguration:")
    print(f"  Pi IP Address: {PI_IP_ADDRESS}")
    print(f"  MQTT Port:     {MQTT_PORT}")
    print(f"  MQTT Topic:    {MQTT_TOPIC}")
    print("="*70)
    
    # Create MQTT client
    client = mqtt.Client()
    
    # Assign callbacks
    client.on_connect = on_connect
    client.on_message = on_message
    client.on_disconnect = on_disconnect
    
    # Connect and run
    try:
        print(f"\nConnecting to {PI_IP_ADDRESS}:{MQTT_PORT}...")
        client.connect(PI_IP_ADDRESS, MQTT_PORT, 60)
        
        # Start network loop (blocks until interrupted)
        client.loop_forever()
        
    except KeyboardInterrupt:
        # User pressed Ctrl+C
        print("\n\n" + "="*70)
        print("  SESSION ENDED")
        print("="*70)
        
        # Display statistics
        if session_start_time:
            duration = (datetime.now() - session_start_time).total_seconds()
            rate = message_count / duration if duration > 0 else 0
            
            print(f"\nSession Statistics:")
            print(f"  Total Messages:    {message_count}")
            print(f"  Session Duration:  {duration:.1f} seconds")
            print(f"  Average Rate:      {rate:.2f} messages/second")
        
        print("\n" + "="*70)
        client.disconnect()
        
    except ConnectionRefusedError:
        print("\nCONNECTION REFUSED")
        
    except Exception as e:
        print(f"\nERROR: {e}")


# Entry Point

if __name__ == "__main__":
    main()
