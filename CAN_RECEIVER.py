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

#can_bus queue for receiving CAN messages from a separate thread
can_buffer = queue.Queue(maxsize=1000)

#Initialize CAN bus (Linux SocketCAN example, adjust for your platform and CAN interface)
bus = can.interface.Bus(bustype='socketcan', channel='can0', bitrate=500000)

def can_listener():
    while True:
        try:
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

#Start CAN listener thread
listener_thread = threading.Thread(target=can_listener, daemon=True)
listener_thread.start()

while True:
    msg_to_process = get_can_message()
    if msg_to_process:
        print(f"Main_Thread: Processing CAN message: {msg_to_process}")
    else:
        print("Main_Thread: No CAN message received, continuing with other tasks")