import time
from collections import deque

import paho.mqtt.client as mqtt
import matplotlib.pyplot as plt
import matplotlib.animation as animation

# =========================
# MQTT CONFIG
# =========================
PI_IP_ADDRESS = "10.42.0.1"
MQTT_PORT = 1883
MQTT_TOPIC_PEDAL = "pedal"

# =========================
# DATA STORAGE
# =========================
WINDOW_SECONDS = 60
MAX_POINTS = 300

timestamps = deque(maxlen=MAX_POINTS)
values = deque(maxlen=MAX_POINTS)
current_value = 0.0

# =========================
# MQTT CALLBACKS
# =========================
def on_connect(client, userdata, flags, rc):
    print("Connected:", rc)
    client.subscribe(MQTT_TOPIC_PEDAL)

def parse_payload(payload):
    try:
        return float(payload)
    except Exception:
        try:
            import json
            return float(json.loads(payload)["value"])
        except Exception:
            return None

/* def on_message(client, userdata, msg):
    global current_value
    payload = msg.payload.decode()
    val = parse_payload(payload)
    if val is None:
        print("Bad payload:", payload)
        return
    current_value = val
    timestamps.append(time.time())
    values.append(val)

# =========================
# MQTT SETUP
# =========================
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect(PI_IP_ADDRESS, MQTT_PORT, 60)
client.loop_start()

# =========================
# PLOT SETUP
# =========================
plt.style.use("dark_background")
fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(8, 8))

NEON_RED   = "#ff2d2d"
NEON_GREEN = "#00ff9f"
GRID       = "#2a2a2a"

# ---- CURRENT VALUE DISPLAY ----
ax1.set_facecolor("#0a0a0a")
ax1.set_title("PEDAL INPUT", fontsize=14, color="white")
ax1.axis("off")
text_display = ax1.text(
    0.5, 0.5, "0.00",
    fontsize=48,
    ha="center",
    va="center",
    color=NEON_GREEN,
    fontweight="bold",
    transform=ax1.transAxes,
)

# ---- TIME SERIES ----
ax2.set_facecolor("#0a0a0a")
ax2.set_title("LAST 60 SECONDS", color="white")
ax2.set_xlim(0, WINDOW_SECONDS)
ax2.set_ylim(0, 100)           # ← sensible default before data arrives
ax2.grid(color=GRID, linestyle="--", linewidth=0.5)
ax2.tick_params(colors="white")
line, = ax2.plot([], [], color=NEON_RED, linewidth=2)

# =========================
# UPDATE LOOP
# =========================
def update(frame):
    now = time.time()

    x = []
    y = []
    for t, v in zip(timestamps, values):
        if now - t <= WINDOW_SECONDS:
            x.append(t - now + WINDOW_SECONDS)
            y.append(v)

    line.set_data(x, y)
    ax2.set_xlim(0, WINDOW_SECONDS)

    if y:
        ymin, ymax = min(y), max(y)
        if ymin == ymax:
            ymax = ymin + 1
        ax2.set_ylim(ymin * 0.9, ymax * 1.1)
    else:
        ax2.set_ylim(0, 100)   # ← hold default when no data yet

    # Color shift
    if current_value > 75:
        text_display.set_color("#ff2d2d")
    elif current_value > 40:
        text_display.set_color("#ffd60a")
    else:
        text_display.set_color("#00ff9f")

    text_display.set_text(f"{current_value:.2f}")

# blit=False lets axis limit changes redraw properly
ani = animation.FuncAnimation(fig, update, interval=200, blit=False)

plt.tight_layout()
plt.show()