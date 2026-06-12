"""
VCU Capstone - Pit Telemetry Dashboard
PSU Viking Motorsports | FSAE Electric 2027 | Telemetry System

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
QUICK START (how to run the dashboard):
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  1. Install dependencies (first time only):
       pip install flask flask-socketio paho-mqtt

  2. Make sure you're connected to the same network
     as the Raspberry Pi sending MQTT data (PI_IP_ADDRESS below).

  3. Run this file:
       python dashboard.py

  4. Open your browser and go to:
       http://localhost:5000

  That's it — the dashboard will auto-update as
  telemetry data streams in over MQTT.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
HOW IT WORKS (high-level):
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  - Flask serves the web dashboard (index.html)
  - MQTT client subscribes to the telemetry topic
    and listens for incoming data from the car
  - When data arrives, it's forwarded to the browser
    in real time via SocketIO (WebSockets)
  - The browser-side JS (dashboard.js) receives the
    data and updates the display

  Data flow:
    Car sensor → Pi (MQTT broker) → dashboard.py → SocketIO → dashboard.js → index.html display

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
OFFLINE / NO PI MODE:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  If the Pi isn't reachable, the script will print
  a warning and run in offline mode — the dashboard
  will still load but won't receive live data.
  Use this mode to work on the UI without hardware.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
TROUBLESHOOTING:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  Value isn't updating  → check dashboard.js
  Layout needs changing → check index.html
  Color/style issues    → check style.css
  MQTT not connecting   → confirm Pi IP below and
                          that you're on same network
"""

from flask import Flask, render_template
from flask_socketio import SocketIO
import paho.mqtt.client as mqtt
import json

# ── App Setup ──────────────────────────────────────────────────────────────────
# Flask is the web server; SocketIO enables real-time push to the browser.
# cors_allowed_origins="*" lets any browser origin connect (fine for local use).
app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*")

# ── MQTT Configuration ─────────────────────────────────────────────────────────
# Update PI_IP_ADDRESS if Raspberry Pi gets a new IP on the network.
# MQTT_TOPIC must match what the Pi is publishing to.
PI_IP_ADDRESS = "10.42.0.1"       # Pi IP (hotspot gateway address) 
MQTT_PORT     = 1883              # Default MQTT port (no TLS)
MQTT_TOPIC    = "pedal"            # Main telemetry topic — change to match Pi config

# ── Serve the Dashboard ────────────────────────────────────────────────────────
# This route handles GET requests to http://localhost:5000/
# Flask looks for index.html inside the /templates folder automatically.
@app.route('/')
def dashboard():
    return render_template('index.html')

# ── MQTT Callbacks ─────────────────────────────────────────────────────────────
# These three functions are triggered automatically by the MQTT client library.

def on_connect(client, userdata, flags, rc):
    """Called when the MQTT client connects to the broker (Pi).
    rc=0 means success; any other value is an error code."""
    if rc == 0:
        print("Connected to MQTT broker")
        client.subscribe(MQTT_TOPIC)          # Start listening for telemetry
        print(f"Subscribed to topic: {MQTT_TOPIC}")
    else:
        print(f"MQTT connection failed, code: {rc}")

def on_message(client, userdata, msg):
    """Called every time a new message arrives on the subscribed topic.
    Expects JSON payload — decodes it and forwards it to the browser via SocketIO.
    The browser-side listener is in dashboard.js (socket.on('telemetry_update', ...))"""
    try:
        data = json.loads(msg.payload.decode())
        print(f"Data received: {data}")
        socketio.emit('telemetry_update', data)   # Push to all connected browser clients
    except Exception as e:
        print(f"Error parsing message: {e}")    # Likely malformed JSON from the Pi

def on_disconnect(client, userdata, rc):
    """Called if the MQTT connection drops (Pi turned off, network issue, etc.)"""
    print("Disconnected from MQTT broker")

# ── Start MQTT Client ──────────────────────────────────────────────────────────
# Registers the callbacks above, then attempts to connect to the Pi.
mqtt_client = mqtt.Client()
mqtt_client.on_connect    = on_connect
mqtt_client.on_message    = on_message
mqtt_client.on_disconnect = on_disconnect

def start_mqtt():
    """Tries to connect to the MQTT broker. If the Pi isn't reachable,
    falls back to offline mode so the dashboard still loads for UI work."""
    try:
        print(f"Connecting to MQTT broker at {PI_IP_ADDRESS}:{MQTT_PORT}...")
        mqtt_client.connect(PI_IP_ADDRESS, MQTT_PORT, 60)  # 60s keepalive
        mqtt_client.loop_start()   # Runs MQTT network loop in a background thread
    except Exception as e:
        print(f"Could not connect to MQTT broker: {e}")
        print("Running in offline mode (no live data)")

# ── Main ───────────────────────────────────────────────────────────────────────
if __name__ == '__main__':
    print("=" * 50)
    print("  PSU Viking Motorsports — Pit Telemetry")
    print("  FSAE Electric 2027 | System 4")
    print("=" * 50)
    start_mqtt()
    print("\n Dashboard running at: http://localhost:5000")
    print(" Open that URL in your browser\n")
    # host='0.0.0.0' makes it accessible on your local network too,
    # not just localhost — useful if someone wants to view from another device.
    socketio.run(app, host='0.0.0.0', port=5000, debug=True)
