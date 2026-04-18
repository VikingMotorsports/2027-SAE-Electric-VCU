import can
import threading
import time

# -------------------------
# Shared CAN bus (ONE instance)
# -------------------------
bus = can.interface.Bus(
    channel='COM5',
    interface='slcan',
    bitrate=500000
)

# -------------------------
# RX THREAD
# -------------------------
def rx_loop():
    print("RX thread started")

    while True:
        msg = bus.recv(timeout=1.0)  # non-blocking-ish
        if msg:
            print(f"RX: {msg}")

# -------------------------
# TX FUNCTION (called from main thread)
# -------------------------
def send_message():
    msg = can.Message(
        arbitration_id=0x123,
        data=[0xDE, 0xAD, 0xBE, 0xEF],
        is_extended_id=False
    )
    bus.send(msg)
    print("TX sent")

# -------------------------
# START RX THREAD
# -------------------------
rx_thread = threading.Thread(target=rx_loop, daemon=True)
rx_thread.start()

# -------------------------
# MAIN LOOP (TX control)
# -------------------------
print("Press Enter to send messages. Ctrl+C to exit.")

while True:
    input()
    send_message()

bus.shutdown()