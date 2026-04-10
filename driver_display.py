'''
                    Driver Display
    This code simulates a simple 6-speed gearbox 
        with a rev limiter and shift lights.
    
                @author: Allen Paul
                @date: 03/30/2026
                @title: driver_display.py

'''

import matplotlib.pyplot as plt
import serial as ser # Replaced import
import time
import numpy as np

# --- 1. SERIAL SETUP ---
try:
    port = ser.Serial('COM3', 11500, timeout=0) # timeout=0 makes it non-blocking
except Exception as e:
    print(f"Serial Error: {e}")
    port = None

# --- 2. Mock Engine & State ---
gear_ratios = [0, 3.4, 2.4, 1.9, 1.5, 1.25, 1.05] 
rev_limit = 12500
idle_rpm = 2500
drag_coefficient = 0.00006  
braking_force = 2.5
accel_power = 12.0          

current_gear = 1
velocity = 0.0
rpm = idle_rpm

# Input tracking states (since serial often sends single triggers)
accel_active = 0.0
brake_active = 0.0

# --- 3. DISPLAY SETUP ---
plt.style.use('dark_background')
fig, ax = plt.subplots(figsize=(8, 5))
plt.ion()

print("Serial Control Active. (Expects: 'W'=Gas, 'B'=Brake, 'U'=Up, 'S'=Down, 'X'=Quit)")

try:
    while True:
        # --- A. SERIAL INPUT HANDLING ---
        if port and port.in_waiting > 0:
            # Read one byte and convert to uppercase string
            command = port.read().decode('utf-8').upper()
            
            # Simple Toggle or Trigger Logic
            if command == 'W': accel_active = 1.0 if accel_active == 0.0 else 0.0
            if command == 'B': brake_active = braking_force if brake_active == 0.0 else 0.0
            
            if command == 'U' and current_gear < 6:
                current_gear += 1
            if command == 'S' and current_gear > 1: # 'S' for Space/Down
                current_gear -= 1
            if command == 'X': break # Exit command

        # --- B. ENGINE MATH ---
        rpm = idle_rpm + (velocity * gear_ratios[current_gear] * 36.5)
        power_cut = 0.0 if rpm >= (rev_limit - 50) else 1.0
        turbo_boost = (rpm / rev_limit) ** 1.1
        engine_push = (accel_active * accel_power * turbo_boost * power_cut) / (gear_ratios[current_gear] ** 0.4)
        
        resistance = (velocity ** 2) * drag_coefficient
        velocity = max(0, velocity + (engine_push - resistance - brake_active))
        kmh = int(velocity)

        if rpm >= rev_limit:
            rpm = rev_limit - np.random.randint(300, 600)

        # --- C. DASHBOARD RENDERING ---
        ax.clear()
        ax.text(0.5, 0.45, str(current_gear), fontsize=110, ha='center', va='center', color='white', fontweight='bold')
        ax.text(0.5, -0.05, f"{kmh}", fontsize=60, ha='center', color="#FF0000", fontweight='bold')
        
        # Shift Lights
        num_dots = 12
        rpm_step = rev_limit / num_dots
        for i in range(num_dots):
            dot_x = 0.17 + (i * 0.06) 
            dot_rpm = (i + 1) * rpm_step
            dot_color = '#222222'
            if rpm >= dot_rpm:
                dot_color = "#CA0CB1" if dot_rpm < 10500 else ('red' if (time.time() * 20) % 2 > 1 else '#111111')
            ax.plot(dot_x, 0.85, 'o', markersize=14, color=dot_color)

        # Telemetry Bars
        ax.bar(0.9, accel_active, color='lime', width=0.06)
        ax.bar(0.1, min(1.0, brake_active/braking_force), color='red', width=0.06)

        ax.set_xlim(0, 1)
        ax.set_ylim(-0.4, 1.0)
        ax.axis('off')
        plt.draw()
        plt.pause(0.001)

except KeyboardInterrupt:
    pass
finally:
    if port: port.close()
    plt.close()