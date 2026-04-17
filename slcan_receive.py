"""
test _ script not confirmed working yet
"""

import can
import time

channel = "/dev/ttyACM0"
bitrate = 500000

bus = can.interface.Bus(channel=channel, bustype = "slcan", bitrate=bitrate)

timeout = 10
end_time = time.time() + timeout

print("Listening...")

try:
    while 1:
        message = bus.recv()
        time.sleep(0.01)
        if message:
            print(f"Message: {message}\n")

except KeyboardInterrupt:
    bus.shutdown()
