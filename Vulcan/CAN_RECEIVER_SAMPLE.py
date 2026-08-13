import pygame
import pygame.gfxdraw
import math
import time
import random
import sys
import can
import threading
import queue

# ─────────────────────────────────────────────────────────────────────────────
#  CAN INPUT SETUP
# ─────────────────────────────────────────────────────────────────────────────

# can_bus queue for receiving CAN messages from a separate thread
can_buffer = queue.Queue(maxsize=1000)

# Initialize CAN bus (Linux SocketCAN example, adjust for your platform and CAN interface)
bus = can.interface.Bus(interface='socketcan', channel='can0', bitrate=500000)

def can_listener():
    while True:
        try:
            print(bus)
            msg = bus.recv() #get message from CAN bus (blocking)
            print(f"Listener_Thread: Received CAN message: {msg}")
            can_buffer.put(msg, block=False)
        except queue.Full:
            print("Buffer full, dropping CAN message")

def get_can_message():
    try:
        return can_buffer.get(block=True, timeout=1) #get message from buffer (non-blocking)
    except queue.Empty:
        return None

# Start CAN listener thread
listener_thread = threading.Thread(target=can_listener, daemon=True)
listener_thread.start()

while True:
    msg = get_can_message()
    if msg:
        print("ID:",hex(msg.arbitration_id))
        print("DLC:", msg.dlc)
        print("DATA: 0x", msg.data.hex().upper())
    else:
        print("Main_Thread: No CAN message received, continuing with other tasks")


#    msg = parse_can_message(msg_to_process)

#    print(f"ID: 0x{msg.can_id:X}")
#    print("DLC:", msg.dlc)
#    print("DATA:", msg.data.hex().upper())

