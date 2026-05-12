import time
import json
from collections import deque

import paho.mqtt.client as mqtt
import matplotlib.pyplot as plt
import matplotlib.animation as animation

# WORKING PLOT USED FOR DEMO
# =========================
# CONFIG
# =========================
BROKER = "10.42.0.1"   # your Pi
PORT = 1883
TOPIC = "pedal"

WINDOW_SECONDS = 60
MAX_POINTS = 300

# =========================
# DATA STORAGE
# =========================
timestamps = deque(maxlen=MAX_POINTS)
values = deque(maxlen=MAX_POINTS)

current_value = 0.0

# =========================
# MQTT CALLBACKS
# =========================
def on_connect(client, userdata, flags, rc):
    print("Connected:", rc)
    client.subscribe(TOPIC)

def on_message(client, userdata, msg):
    global current_value

    payload = msg.payload.decode()
    print("RAW:", payload)

    try:
        val = float(json.loads(payload)["accelerator"])
    except:
        print("Bad payload")
        #return

    # Clamp between 0 and 1 (safety)
    val = max(0.0, min(1.0, val))

    current_value = val

    timestamps.append(time.time())
    values.append(val)

# =========================
# MQTT SETUP
# =========================
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect(BROKER, PORT, 60)
client.loop_start()

# =========================
# PLOT SETUP
# =========================
fig, (ax_text, ax_plot) = plt.subplots(2, 1, figsize=(8, 8))

# ---- TEXT DISPLAY ----
ax_text.axis("off")

text_display = ax_text.text(
    0.5, 0.6, "0.00",
    fontsize=40,
    ha="center",
    va="center"
)

percent_display = ax_text.text(
    0.5, 0.3, "0%",
    fontsize=25,
    ha="center",
    va="center"
)

# ---- GRAPH ----
line, = ax_plot.plot([], [], linewidth=2)

ax_plot.set_title("Value Over Time (Last 60s)")
ax_plot.set_xlim(0, WINDOW_SECONDS)
ax_plot.set_ylim(0, 1)

ax_plot.set_xlabel("Time (seconds)")
ax_plot.set_ylabel("Value (0–1)")

# =========================
# UPDATE FUNCTION
# =========================
def update(frame):
    now = time.time()

    # Filter last 60 seconds
    data = [
        (t, v) for t, v in zip(timestamps, values)
        if (now - t <= WINDOW_SECONDS)
    ]

    if data:
        x = [t - now + WINDOW_SECONDS for t, v in data]
        y = [v for t, v in data]

        line.set_data(x, y)

    # Update text
    percent = current_value * 100
    text_display.set_text(f"{current_value:.2f}")
    percent_display.set_text(f"{percent:.0f}%")

    return line, text_display, percent_display

# =========================
# RUN
# =========================
ani = animation.FuncAnimation(
    fig,
    update,
    interval=50,
    cache_frame_data=False
)

plt.tight_layout()
plt.show()
